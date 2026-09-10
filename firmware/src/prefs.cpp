#include "prefs.h"
#include "touch.h"
#include "lang.h"

#include <Arduino.h>
#include <Preferences.h>

static Preferences store;
static PrefsState state{};
static unsigned long alarmDeadline = 0;
static unsigned long lastRemainSave = 0;

void prefsApplyTouchMap() {
  touchSetTransform((state.touchMap & 1) != 0, (state.touchMap & 2) != 0, false);
}

void prefsBegin() {
  store.begin("bondwatch", false);
  state.brightness = store.getUChar("bright", BRIGHT_MID);
  state.volume = store.getUChar("vol", VOL_MID);
  state.dnd = store.getBool("dnd", false);
  state.landscape = store.getBool("land", false);
  state.vad = store.getUChar("vad", VAD_OFF);
  // One-shot: older builds defaulted Microphone to Auto and kept re-entering
  // listen/think/speak. Force Off once after this firmware.
  if (!store.getBool("vadOff1", false)) {
    state.vad = VAD_OFF;
    store.putUChar("vad", VAD_OFF);
    store.putBool("vadOff1", true);
  }
  state.lang = store.getUChar("lang", LANG_ZH);
  // Default: flipX+flipY — Waveshare 2.0 CST816D often mirrors both axes vs ST7789.
  state.touchMap = store.getUChar("tmap", 0x03);
  // B7: drop leftover swapXY and keep only the two flips.
  if (!store.getBool("tui1", false)) {
    state.touchMap = static_cast<uint8_t>(state.touchMap & 0x03);
    if (state.touchMap == 0) {
      state.touchMap = 0x03;
    }
    store.putUChar("tmap", state.touchMap);
    store.putBool("tui1", true);
  }
  state.alarmRemainSec = store.getUInt("alarmSec", 0);
  if (state.brightness > BRIGHT_HIGH) {
    state.brightness = BRIGHT_MID;
  }
  if (state.volume > VOL_HIGH) {
    state.volume = VOL_MID;
  }
  if (state.vad > VAD_SENSITIVE) {
    state.vad = VAD_OFF;
  }
  if (state.lang > LANG_EN) {
    state.lang = LANG_ZH;
  }
  state.touchMap = static_cast<uint8_t>(state.touchMap & 0x03);
  if (state.alarmRemainSec > 0) {
    alarmDeadline = millis() + state.alarmRemainSec * 1000UL;
  }
  prefsApplyTouchMap();
  Serial.println("prefs: loaded");
  prefsPrint();
}

PrefsState &prefs() {
  return state;
}

void prefsSave() {
  store.putUChar("bright", state.brightness);
  store.putUChar("vol", state.volume);
  store.putBool("dnd", state.dnd);
  store.putBool("land", state.landscape);
  store.putUChar("vad", state.vad);
  store.putUChar("lang", state.lang);
  store.putUChar("tmap", state.touchMap);
  store.putUInt("alarmSec", state.alarmRemainSec);
}

void prefsPrint() {
  Serial.printf("prefs bright=%s vol=%s dnd=%d land=%d vad=%s lang=%s touch=%s alarmSec=%lu\n",
                prefsBrightnessName(), prefsVolumeName(), state.dnd ? 1 : 0, state.landscape ? 1 : 0,
                prefsVadName(), prefsLangName(), prefsTouchMapName(),
                static_cast<unsigned long>(state.alarmRemainSec));
}

uint16_t prefsVadThreshold() {
  switch (state.vad) {
    case VAD_OFF:
      return 65535; // auto-listen disabled
    case VAD_SENSITIVE:
      return 140;
    case VAD_NORMAL:
    default:
      return 220;
  }
}

uint8_t prefsBrightnessDuty() {
  switch (state.brightness) {
    case BRIGHT_LOW:
      return 48;
    case BRIGHT_HIGH:
      return 255;
    case BRIGHT_MID:
    default:
      return 160;
  }
}

const char *prefsBrightnessName() {
  switch (state.brightness) {
    case BRIGHT_LOW:
      return tr("低", "Low");
    case BRIGHT_HIGH:
      return tr("高", "High");
    case BRIGHT_MID:
    default:
      return tr("中", "Mid");
  }
}

const char *prefsVolumeName() {
  switch (state.volume) {
    case VOL_MUTE:
      return tr("静音", "Mute");
    case VOL_LOW:
      return tr("低", "Low");
    case VOL_HIGH:
      return tr("高", "High");
    case VOL_MID:
    default:
      return tr("中", "Mid");
  }
}

void prefsCycleVolume() {
  state.volume = static_cast<uint8_t>((state.volume + 1) % 4);
  prefsSave();
  Serial.printf("volume=%s\n", prefsVolumeName());
}

const char *prefsTouchMapName() {
  static char buf[8];
  snprintf(buf, sizeof(buf), "%c%c", (state.touchMap & 1) ? 'X' : '-', (state.touchMap & 2) ? 'Y' : '-');
  return buf;
}

void prefsCycleTouchMap() {
  state.touchMap = static_cast<uint8_t>((state.touchMap + 1) & 3);
  prefsSave();
  prefsApplyTouchMap();
  Serial.printf("touch map=%s (X=flipX Y=flipY)\n", prefsTouchMapName());
}

void prefsInvertTouchMap() {
  state.touchMap = static_cast<uint8_t>((state.touchMap ^ 0x03) & 0x03);
  prefsSave();
  prefsApplyTouchMap();
  Serial.printf("touch invert 180 -> %s\n", prefsTouchMapName());
}

const char *prefsVadName() {
  switch (state.vad) {
    case VAD_OFF:
      return tr("关闭", "Off");
    case VAD_SENSITIVE:
      return tr("灵敏", "Auto+");
    case VAD_NORMAL:
    default:
      return tr("自动", "Auto");
  }
}

const char *prefsLangName() {
  return state.lang == LANG_EN ? "English" : "中文";
}

void prefsCycleLang() {
  state.lang = state.lang == LANG_EN ? LANG_ZH : LANG_EN;
  prefsSave();
  Serial.printf("lang=%s\n", prefsLangName());
}

void prefsArmAlarmMinutes(uint32_t minutes) {
  prefsArmAlarmSeconds(minutes * 60UL);
}

void prefsArmAlarmSeconds(uint32_t seconds) {
  if (seconds == 0) {
    prefsCancelAlarm();
    return;
  }
  state.alarmRemainSec = seconds;
  alarmDeadline = millis() + state.alarmRemainSec * 1000UL;
  prefsSave();
  Serial.printf("alarm in %lu sec\n", static_cast<unsigned long>(seconds));
}

void prefsCancelAlarm() {
  state.alarmRemainSec = 0;
  alarmDeadline = 0;
  prefsSave();
  Serial.println("alarm cancel");
}

bool prefsAlarmArmed() {
  return alarmDeadline != 0;
}

unsigned long prefsAlarmDeadline() {
  return alarmDeadline;
}

void prefsAlarmTick(unsigned long now) {
  if (alarmDeadline == 0) {
    return;
  }
  if (now - lastRemainSave < 1000) {
    return;
  }
  lastRemainSave = now;
  if (now >= alarmDeadline) {
    state.alarmRemainSec = 0;
  } else {
    state.alarmRemainSec = static_cast<uint32_t>((alarmDeadline - now) / 1000UL);
  }
  store.putUInt("alarmSec", state.alarmRemainSec);
}

bool prefsAlarmDue(unsigned long now) {
  if (alarmDeadline == 0) {
    return false;
  }
  if (now < alarmDeadline) {
    return false;
  }
  alarmDeadline = 0;
  state.alarmRemainSec = 0;
  prefsSave();
  return true;
}
