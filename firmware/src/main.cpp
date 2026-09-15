#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <stdio.h>

#include "pins.h"
#include "touch.h"
#include "ui.h"
#include "sim.h"
#include "net.h"
#include "mic.h"
#include "secrets.h"
#include "prefs.h"
#include "app.h"
#include "lang.h"
#include "lte.h"

enum Phase : uint8_t { PHASE_IDLE, PHASE_LISTEN, PHASE_THINK, PHASE_SPEAK, PHASE_ALARM };

static const unsigned long TAP_LISTEN_MS = 1600;
static const unsigned long SPEAK_MS = 3500;
static const unsigned long ALARM_MS = 4000;
static const unsigned long LONG_PRESS_MS = 800;
static const unsigned long DOUBLE_MS = 350;
static const unsigned long VAD_COOLDOWN_MS = 5000;

static Phase phase = PHASE_IDLE;
static unsigned long phaseAt = 0;
static bool screenOn = true;
static bool pttHeld = false;
static unsigned long lastVadAt = 0;
static uint16_t listenRms = 0;
static char lastHeard[48] = "Hi, BondWatch";
static CloudReply lastReply{};

static int lastPwr = HIGH;
static unsigned long pwrDownAt = 0;
static unsigned long pwrUpAt = 0;
static uint8_t pwrClicks = 0;

static void audioListen(bool on) {
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#if VOICE_FEATURES
  if (on) {
    micBegin();
  } else {
    micEnd();
    uiSetMicLevel(0);
  }
#else
  (void)on;
  micEnd();
  uiSetMicLevel(0);
#endif
}

static void audioSpeak(bool on) {
  const bool audible = on && !prefs().dnd && prefs().volume != VOL_MUTE;
  digitalWrite(PIN_LED, audible ? HIGH : LOW);
}

static Emotion mapReplyEmotion(const CloudReply &reply) {
  if (!reply.ok) {
    return EMO_OFFLINE;
  }
  return prefs().dnd ? EMO_SILENT : reply.emotion;
}

static void fillOfflineReply(uint16_t rms) {
  static uint8_t idx = 0;
  static const char *loudZh[] = {"听到了！", "声音不错", "很清楚", "收到", "真有意思"};
  static const char *loudEn[] = {"I hear you!", "Nice voice", "Loud and clear", "Got that", "You sound fun"};
  static const char *softZh[] = {"有点轻…", "再说一次？", "我在听", "几乎没声", "靠近一点"};
  static const char *softEn[] = {"Hmm soft...", "Say again?", "I'm listening", "Almost quiet", "Come closer"};
  lastReply.ok = true;
  const bool heard = rms > prefsVadThreshold();
  lastReply.emotion = heard ? EMO_SPEAK : EMO_QUIET;
  const char **table = heard ? (langIsEn() ? loudEn : loudZh) : (langIsEn() ? softEn : softZh);
  const uint8_t n = 5;
  strncpy(lastReply.text, table[idx % n], sizeof(lastReply.text) - 1);
  lastReply.text[sizeof(lastReply.text) - 1] = 0;
  idx++;
}

static unsigned long talkCooldownUntil = 0;

static void enterPhase(Phase next, const char *why);

static unsigned long homeTapAckUntil = 0;

// 0=offline 1=connecting 2=online — debounce home UI on link changes
static uint8_t lastNetUi = 255;

static uint8_t netUiState() {
  if (!netWifiOk()) {
    return 0;
  }
  if (!netReady()) {
    return 1;
  }
  return 2;
}

static void applyIdleHomeUi() {
  if (prefs().dnd) {
    uiShow(EMO_SILENT, tr("勿扰已开", "DND on"));
    return;
  }
  switch (netUiState()) {
    case 0:
      uiShow(EMO_OFFLINE, tr("没网 · 闹钟仍可用", "Offline · alarms OK"));
      break;
    case 1:
      uiPatchSubtitle(tr("连接中…", "Connecting..."));
      break;
    default:
      uiPatchSubtitle(tr("右滑菜单 · 点按钮", "swipe right · tap"));
      break;
  }
}

static void syncNetUi() {
  if (!screenOn || appScreen() != APP_HOME || homeTapAckUntil) {
    return;
  }
  if (phase != PHASE_IDLE && phase != PHASE_ALARM) {
    return;
  }

  const uint8_t s = netUiState();
  if (s == lastNetUi) {
    return;
  }
  lastNetUi = s;

  if (phase == PHASE_ALARM) {
    return;
  }
  applyIdleHomeUi();
}

static void refreshPairBadge() {
  const char *pc = netPairCode();
  char pairBuf[20];
  if (pc[0] && strcmp(pc, "----") != 0) {
    snprintf(pairBuf, sizeof(pairBuf), "PAIR %s", pc);
    uiSetPair(pairBuf);
  } else {
    uiSetPair("PAIR ----");
  }
}

static void onRemoteFace(const char *emotion, const char *text, const char *source) {
  (void)source;
  if (!screenOn) {
    return;
  }
  appGoHome();
  Emotion emo = EMO_SPEAK;
  if (emotion && !strcmp(emotion, "listen")) emo = EMO_LISTEN;
  else if (emotion && !strcmp(emotion, "think")) emo = EMO_THINK;
  else if (emotion && !strcmp(emotion, "silent")) emo = EMO_SILENT;
  else if (emotion && !strcmp(emotion, "quiet")) emo = EMO_QUIET;
  else if (emotion && !strcmp(emotion, "alarm")) emo = EMO_ALARM;
  else if (emotion && !strcmp(emotion, "offline")) emo = EMO_OFFLINE;
  else if (emotion && !strcmp(emotion, "idle")) emo = EMO_IDLE;
  uiShow(emo, text && text[0] ? text : "hi");
  homeTapAckUntil = millis() + 4000;
}

static unsigned long lastHomeTapAt = 0;

static void onHomeTap() {
  if (!screenOn || appScreen() != APP_HOME) {
    return;
  }
  const unsigned long now = millis();
  if (now - lastHomeTapAt < 500) {
    return;
  }
  lastHomeTapAt = now;
  uiSetRecord(false, 0, nullptr);
#if !OFFLINE_USB
  if (!netWifiOk() || !netCloudUp()) {
    uiPatchSubtitle(tr("没网 · 滑动仍可用", "Offline · swipes OK"));
    homeTapAckUntil = millis() + 800;
    Serial.println("home tap offline");
    return;
  }
#endif
  uiPatchSubtitle(tr("点一下 · 语音已关", "Hi · I am here"));
  homeTapAckUntil = millis() + 800;
  Serial.println("home tap ack");
}

static void startTalk(const char *why) {
#if !VOICE_FEATURES
  (void)why;
  appStartTalk();
  return;
#else
  if (millis() < talkCooldownUntil) {
    Serial.printf("talk ignored (%s) cooldown\n", why);
    return;
  }
  if (appScreen() != APP_HOME) {
    appGoHome();
  }
  // Only accept intentional talk while already idle on home.
  if (phase != PHASE_IDLE && phase != PHASE_ALARM) {
    Serial.printf("talk ignored (%s) busy\n", why);
    return;
  }
  pttHeld = false;
  listenRms = 0;
  enterPhase(PHASE_LISTEN, why);
#endif
}

static void goIdle(const char *why) {
  enterPhase(PHASE_IDLE, why);
}

static void onDndChanged(bool on) {
  Serial.println(on ? "DND on" : "DND off");
  if (screenOn && appScreen() == APP_HOME) {
    enterPhase(PHASE_IDLE, on ? "Mute / DND" : "DND off");
  }
}

static void onLandscapeChanged(bool on) {
  prefs().landscape = on;
  simSetLandscape(on);
  uiSetLandscape(on);
  Serial.printf("orient %s %dx%d\n", on ? "landscape" : "portrait", uiWidth(), uiHeight());
  if (!screenOn) {
    return;
  }
  if (appScreen() == APP_HOME) {
    if (phase == PHASE_LISTEN || phase == PHASE_THINK || phase == PHASE_SPEAK) {
      return;
    }
    enterPhase(phase == PHASE_ALARM ? PHASE_ALARM : PHASE_IDLE, "orient");
  } else {
    appRedraw();
  }
}

static void blankScreen() {
  if (!screenOn) {
    return;
  }
  screenOn = false;
  uiSetBacklight(false);
  audioSpeak(false);
  audioListen(false);
  uiBlank();
  phase = PHASE_IDLE;
  Serial.println("Screen off");
}

static void toggleScreenPower() {
  if (screenOn) {
    blankScreen();
  } else {
    screenOn = true;
    uiSetBacklight(true);
    appNoteActivity(millis());
    enterPhase(PHASE_IDLE, "Screen on");
  }
}

static bool phaseIsIdle() {
  return phase == PHASE_IDLE || phase == PHASE_ALARM;
}

static bool phaseBusy() {
  return phase == PHASE_LISTEN || phase == PHASE_THINK || phase == PHASE_SPEAK;
}

static bool screenIsOn() {
  return screenOn;
}

static void enterPhase(Phase next, const char *why) {
  phase = next;
  phaseAt = millis();
  Serial.println(why);
  uiSetLandscape(prefs().landscape);
  uiSetFlags(netReady(), simLteOn(), prefs().dnd);

  switch (next) {
    case PHASE_LISTEN:
      audioListen(true);
      uiShow(EMO_LISTEN, pttHeld ? tr("按住说话…", "Hold talk...") : tr("正在听…", "Listening..."));
      break;
    case PHASE_THINK: {
      audioListen(false);
      audioSpeak(false);
      uiShow(EMO_THINK, tr("思考中…", "Thinking..."));
      uint16_t rms = listenRms;
      // Prefer samples gathered during LISTEN; do not reopen the mic here.
      Serial.printf("mic rms=%u\n", rms);
      if (rms > prefsVadThreshold()) {
        strncpy(lastHeard, tr("听到了", "I heard you"), sizeof(lastHeard) - 1);
      } else {
        strncpy(lastHeard, tr("你好呀", "Hi there"), sizeof(lastHeard) - 1);
      }
      lastHeard[sizeof(lastHeard) - 1] = 0;

#if OFFLINE_USB
      fillOfflineReply(rms);
#else
      lastReply = netTurn(lastHeard);
      if (!lastReply.ok) {
        fillOfflineReply(rms);
      }
#endif
      Serial.printf("reply: %s\n", lastReply.text);
      phase = PHASE_SPEAK;
      phaseAt = millis();
      audioSpeak(!prefs().dnd);
      uiShow(mapReplyEmotion(lastReply), prefs().dnd ? tr("勿扰：仅文字", "DND: text only") : lastReply.text);
      break;
    }
    case PHASE_SPEAK:
      audioSpeak(!prefs().dnd);
      uiShow(mapReplyEmotion(lastReply), prefs().dnd ? tr("勿扰：仅文字", "DND: text only") : lastReply.text);
      break;
    case PHASE_ALARM:
      audioSpeak(true);
      uiShow(EMO_ALARM, tr("本地闹钟", "Local alarm"));
      break;
    case PHASE_IDLE:
    default:
      audioListen(false);
      audioSpeak(false);
      micClearLevel();
      lastNetUi = netUiState();
      applyIdleHomeUi();
      appNotifyPhaseIdle();
      break;
  }
}

static void setScreen(bool on) {
  if (on == screenOn) {
    if (on) {
      appNoteActivity(millis());
    }
    return;
  }
  screenOn = on;
  uiSetBacklight(on);
  if (!on) {
    audioSpeak(false);
    audioListen(false);
    uiBlank();
    phase = PHASE_IDLE;
    Serial.println("Screen off");
  } else {
    appNoteActivity(millis());
    enterPhase(PHASE_IDLE, "Screen on");
  }
}

static void handlePwr(bool down, unsigned long now) {
  // Debounce + ignore boot noise (GPIO chatter looks like "face then black").
  static bool stableDown = false;
  static bool rawDown = false;
  static unsigned long rawChangedAt = 0;
  static const unsigned long DEBOUNCE_MS = 50;
  static const unsigned long MIN_CLICK_MS = 45;
  static const unsigned long BOOT_IGNORE_MS = 3000;

  if (down != rawDown) {
    rawDown = down;
    rawChangedAt = now;
  }

  if ((now - rawChangedAt) >= DEBOUNCE_MS && stableDown != rawDown) {
    const bool wasDown = stableDown;
    stableDown = rawDown;
    lastPwr = stableDown ? LOW : HIGH;

    if (now < BOOT_IGNORE_MS) {
      pwrClicks = 0;
    } else if (!wasDown && stableDown) {
      pwrDownAt = now;
    } else if (wasDown && !stableDown) {
      const unsigned long held = now - pwrDownAt;
      if (held < MIN_CLICK_MS) {
        // bounce
      } else if (held >= LONG_PRESS_MS) {
        pwrClicks = 0;
        setScreen(!screenOn);
      } else {
        pwrClicks++;
        pwrUpAt = now;
      }
    }
  }

  if (pwrClicks > 0 && now - pwrUpAt > DOUBLE_MS) {
    if (pwrClicks >= 2) {
      prefs().dnd = !prefs().dnd;
      prefsSave();
      onDndChanged(prefs().dnd);
    } else {
#if OFFLINE_USB
      // Short PWR was blanking the panel (noise / accidental) — keep screen on while polishing.
      Serial.println("PWR short ignored (USB polish)");
#else
      if (screenOn) {
        setScreen(false);
      } else {
        setScreen(true);
      }
#endif
    }
    pwrClicks = 0;
  }
}

static void handleVol(bool down, unsigned long now) {
  static int lastVol = HIGH;
  static unsigned long volDownAt = 0;

  if (lastVol == HIGH && down) {
    volDownAt = now;
    if (!screenOn) {
      setScreen(true);
    }
  }
  if (lastVol == LOW && !down) {
    const unsigned long held = now - volDownAt;
    if (held >= LONG_PRESS_MS) {
      appOpenSettings();
      Serial.println("VOL long -> settings");
    } else {
      prefsCycleVolume();
      char line[32];
      snprintf(line, sizeof(line), "%s %s", tr("音量", "Vol"), prefsVolumeName());
      if (appScreen() == APP_HOME && (phase == PHASE_IDLE || phase == PHASE_ALARM)) {
        uiPatchSubtitle(line);
      } else if (appScreen() == APP_CONTROL) {
        appRedraw();
      }
    }
  }
  lastVol = down ? LOW : HIGH;
}

static void applyRotate() {
  prefs().landscape = simLandscape();
  prefsSave();
  onLandscapeChanged(prefs().landscape);
}

static void handleSee() {
  // GPIO40 is Wokwi-only. On the real board it is unused / noisy — never start talk from it.
}

static void handleRot() {
  // GPIO42 is Wokwi-only. Ignore on hardware.
}

static void handleMicMonitor(unsigned long now) {
  // Mic only while actively listening — not on idle home or other screens.
  if (!screenOn || appScreen() != APP_HOME || phase != PHASE_LISTEN) {
    return;
  }
  if (touchFingerDown()) {
    return;
  }
  static unsigned long last = 0;
  if (now - last < 100) {
    return;
  }
  last = now;
  const uint16_t rms = micListenMs(12);
  if (rms > listenRms) {
    listenRms = rms;
  }
  uiSetMicLevel(rms);
}

static void handleVad(unsigned long now) {
  // Auto-listen is opt-in only (Settings → Microphone = Auto / Auto+).
  if (prefs().vad == VAD_OFF || prefs().dnd) {
    return;
  }
  if (!screenOn || appScreen() != APP_HOME || phase != PHASE_IDLE) {
    return;
  }
  if (touchSuppressed() || touchFingerDown()) {
    return;
  }
  if (now < talkCooldownUntil || now - lastVadAt < VAD_COOLDOWN_MS) {
    return;
  }
  static unsigned long lastPoll = 0;
  if (now - lastPoll < 800) {
    return;
  }
  lastPoll = now;
  if (!micVoiceDetected(prefsVadThreshold())) {
    micEnd();
    return;
  }
  lastVadAt = now;
  startTalk("VAD");
}

static void handleSerial() {
  char cmd = 0;
  if (!simPollSerial(&cmd)) {
    return;
  }
  if (!screenOn) {
    setScreen(true);
  } else {
    appNoteActivity(millis());
  }
  switch (cmd) {
    case 'm':
    case 'M':
      appRunMicTx();
      break;
    case 't':
    case 'T':
      startTalk("serial talk");
      break;
    case 'u':
    case 'U':
    case 'g':
    case 'G':
      appOpenSettings();
      break;
    case 'c':
    case 'C':
      appOpenControl();
      break;
    case 'v':
    case 'V':
      prefsCycleVolume();
      Serial.printf("vol=%s\n", prefsVolumeName());
      break;
    case 'p':
    case 'P':
      prefsPrint();
      break;
    case '!':
      uiScreenshotToSerial();
      break;
    case 'w':
    case 'W':
      prefs().brightness = BRIGHT_MID;
      prefsSave();
      uiSetBrightnessLevel(BRIGHT_MID);
      uiSetBacklight(true);
      appGoHome();
      enterPhase(PHASE_IDLE, "wake");
      break;
    case 'd':
    case 'D': {
      static bool dbg = false;
      dbg = !dbg;
      touchSetDebug(dbg);
      break;
    }
    case 'o':
    case 'O':
      simSetWifi(false);
      enterPhase(PHASE_IDLE, "Wi-Fi flag down");
      break;
    case 'n':
    case 'N':
      simSetWifi(true);
      netEnsureSession();
      enterPhase(PHASE_IDLE, "Wi-Fi up");
      break;
    case 'l':
    case 'L': {
      Serial.println("LTE self-test...");
      LteTestResult r{};
      lteSelfTest(&r);
      Serial.printf("LTE RESULT at=%d bytes=%d id=%s sim=%s net=%s csq=%d hint=%s\n", r.atOk ? 1 : 0,
                    r.gotBytes ? 1 : 0, r.id, r.sim, r.net, r.csq, r.hint);
      break;
    }
    case '4':
      simSetLte(!simLteOn());
      enterPhase(PHASE_IDLE, simLteOn() ? "4G up" : "4G down");
      break;
    case 'r':
    case 'R': {
      const bool next = !prefs().landscape;
      prefs().landscape = next;
      prefsSave();
      onLandscapeChanged(next);
      break;
    }
    case 's':
    case 'S':
      netPrintStatus();
      prefsPrint();
      break;
    case 'i':
    case 'I':
      appGoHome();
      break;
    case 'h':
    case 'H':
    case '?':
      simPrintHelp();
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(PIN_PWR, INPUT_PULLUP);
  pinMode(PIN_VOL, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  prefsBegin();
  // Low backlight looks like a black crash under room light.
  if (prefs().brightness < BRIGHT_MID) {
    prefs().brightness = BRIGHT_MID;
  }
  prefs().lang = LANG_ZH;
  prefsSave();
  uiBegin();
  uiSetBrightnessLevel(prefs().brightness);
  touchBegin();
  prefsApplyTouchMap();
  touchSetRotation(0);
  touchSuppressMs(600);
  lteBegin();
  simBegin();
  uiSetLandscape(prefs().landscape);
  // Mic stays off — VOICE_FEATURES=0.

  AppHooks hooks{};
  hooks.startTalk = startTalk;
  hooks.goIdle = goIdle;
  hooks.onDndChanged = onDndChanged;
  hooks.onLandscapeChanged = onLandscapeChanged;
  hooks.onHomeTap = onHomeTap;
  hooks.toggleScreen = toggleScreenPower;
  hooks.blankScreen = blankScreen;
  hooks.phaseIsIdle = phaseIsIdle;
  hooks.phaseBusy = phaseBusy;
  hooks.screenIsOn = screenIsOn;
  appBegin(&hooks);
  netOnRemoteFace(onRemoteFace);

  simPrintHelp();

#if OFFLINE_USB
  simSetWifi(false);
  Serial.println("OFFLINE_USB=1  skip WiFi");
#else
  netBegin();
#endif
  enterPhase(PHASE_IDLE, "Hello BondWatch");
}

void loop() {
  const unsigned long now = millis();
  handlePwr(digitalRead(PIN_PWR) == LOW, now);
  handleVol(digitalRead(PIN_VOL) == LOW, now);

  // Drive CST816 state for LVGL pointer. Do not consume swipes here —
  // pages handle tap/swipe themselves.
  TouchGesture gest{};
  while (touchPollGesture(&gest)) {
    appHandleGesture(gest);
  }

  handleSee();
  handleRot();
  handleSerial();
  appTick(now);
#if !OFFLINE_USB
  netTick();
  uiSetFlags(netWifiOk(), simLteOn(), prefs().dnd);
  refreshPairBadge();
  syncNetUi();
#endif

  if (homeTapAckUntil && now >= homeTapAckUntil && screenOn && appScreen() == APP_HOME &&
      phase == PHASE_IDLE) {
    homeTapAckUntil = 0;
    uiPatchSubtitle(prefs().dnd ? tr("勿扰已开", "DND on")
                                : tr("右滑菜单 · 点按钮", "swipe right · tap"));
  }

  const bool touching = touchFingerDown();
#if VOICE_FEATURES
  if (!touching && appScreen() == APP_HOME) {
    if (phase == PHASE_LISTEN) {
      handleMicMonitor(now);
    } else if (phase == PHASE_IDLE) {
      handleVad(now);
    }
  }

  if (screenOn && appScreen() == APP_HOME && phase == PHASE_LISTEN && !pttHeld &&
      now - phaseAt >= TAP_LISTEN_MS) {
    enterPhase(PHASE_THINK, "Listen done");
  }
  if (screenOn && appScreen() == APP_HOME && phase == PHASE_SPEAK && now - phaseAt >= SPEAK_MS) {
    talkCooldownUntil = now + 1200;
    enterPhase(PHASE_IDLE, "Speak done");
  }
#else
  (void)touching;
  if (phase == PHASE_LISTEN || phase == PHASE_THINK || phase == PHASE_SPEAK) {
    enterPhase(PHASE_IDLE, "voice off");
  }
#endif
  if (screenOn && appScreen() == APP_HOME && phase == PHASE_ALARM && now - phaseAt >= ALARM_MS) {
    enterPhase(PHASE_IDLE, "Alarm done");
  }

  if (screenOn) {
    uiTick();
  }

  delay(touchFingerDown() ? 2 : 8);
}
