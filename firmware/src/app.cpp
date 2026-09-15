#include "app.h"

#include "prefs.h"
#include "ui.h"
#include "secrets.h"
#include "lang.h"
#include "lte.h"
#include "mic.h"
#include "net.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

static AppHooks hooks{};
static AppScreen screen = APP_HOME;
static uint8_t settingsScroll = 0;
static uint8_t settingsSel = 0;
static unsigned long lastActivityAt = 0;
static uint8_t calStep = 0;
static bool talkBusy = false;
static unsigned long talkBusyAt = 0;
static unsigned long talkCoolUntil = 0;
static bool talkQueued = false;
static bool talkMicOnly = false;
static void runQueuedTalk();
static void finishTalk();
static void restoreTalkIdleUi();
static void cancelTalk();

static int16_t calRaw0x = 0, calRaw0y = 0;
static int16_t calRaw1x = 0, calRaw1y = 0;
static uint8_t holdOk = 0;
static uint8_t autoInverts = 0;
static uint8_t missFlipVotes = 0;

static const unsigned long AUTO_SLEEP_MS = 0UL;

static bool gestVert(const TouchGesture &g) {
  return g.type == TG_SWIPE_UP || g.type == TG_SWIPE_DOWN;
}

static bool gestHorz(const TouchGesture &g) {
  return g.type == TG_SWIPE_LEFT || g.type == TG_SWIPE_RIGHT;
}

static bool gestTap(const TouchGesture &g) { return g.type == TG_TAP; }

static unsigned long lastUiNavMs = 0;

static bool uiNavTooSoon(unsigned gapMs = 450) {
  const unsigned long now = millis();
  if (now - lastUiNavMs < gapMs) {
    return true;
  }
  lastUiNavMs = now;
  return false;
}

static const char *homeHint() { return tr("点开始对话", "tap Talk"); }

static void drawCurrent() {
  switch (screen) {
    case APP_MENU:
      uiDrawMenu();
      break;
    case APP_CONTROL:
      uiDrawControl(prefs().volume, prefs().brightness, prefs().dnd);
      break;
    case APP_SETTINGS:
      uiDrawSettings(settingsScroll, settingsSel, OFFLINE_USB != 0, touchModeName());
      break;
    case APP_LTE_TEST:
      uiDrawLteTest();
      break;
    case APP_TOUCH_CAL:
      uiDrawTouchCal(calStep, -1, -1, false, prefsTouchMapName());
      break;
    default:
      break;
  }
}

void appRedraw() { drawCurrent(); }

void appNoteActivity(unsigned long now) { lastActivityAt = now; }

void appBegin(const AppHooks *h) {
  if (h) {
    hooks = *h;
  }
  screen = APP_HOME;
  lastActivityAt = millis();
  uiSetBrightnessLevel(prefs().brightness);
  uiSetHint(homeHint());
}

AppScreen appScreen() { return screen; }

void appGoHome() {
  const bool fromOverlay = (screen != APP_HOME);
  if (!fromOverlay) {
    return;
  }
  if (uiNavTooSoon()) {
    return;
  }
  screen = APP_HOME;
  uiResumeHome();
  touchSuppressMs(500);
  if (hooks.goIdle) {
    hooks.goIdle("home");
  }
}

void appOpenMenu() {
  if (uiNavTooSoon()) {
    return;
  }
  screen = APP_MENU;
  touchSuppressMs(500);
  drawCurrent();
}

void appOpenSettings() {
  if (uiNavTooSoon()) {
    return;
  }
  screen = APP_SETTINGS;
  settingsScroll = 0;
  settingsSel = 0;
  touchSuppressMs(500);
  drawCurrent();
}

void appOpenControl() {
  if (uiNavTooSoon()) {
    return;
  }
  screen = APP_CONTROL;
  touchSuppressMs(500);
  drawCurrent();
}

void appOpenLteTest() {
  screen = APP_LTE_TEST;
  uiDrawLteTest();
  uiPatchLteTest(tr("测试中", "testing"), "UART G17/G18", "--", "--", tr("发AT中", "AT..."), 0xFFC107);
  uiTick();
  delay(30);
  uiTick();

  LteTestResult r{};
  lteSelfTest(&r);

  char idLine[48];
  snprintf(idLine, sizeof(idLine), "%s %s", tr("模组", "ID"), r.id[0] ? r.id : "--");
  char simLine[40];
  snprintf(simLine, sizeof(simLine), "%s %s", tr("卡", "SIM"), r.sim[0] ? r.sim : "--");
  char csqLine[40];
  if (r.csq < 0) {
    snprintf(csqLine, sizeof(csqLine), "%s --  %s", tr("信号", "CSQ"), r.net);
  } else {
    snprintf(csqLine, sizeof(csqLine), "%s %d  %s", tr("信号", "CSQ"), r.csq, r.net);
  }

  const char *st = r.atOk ? tr("通过", "PASS") : tr("失败", "FAIL");
  const uint32_t col = r.atOk ? 0x00C853 : 0xF44336;
  uiPatchLteTest(st, idLine, simLine, csqLine, r.hint, col);
  uiTick();
}

void appOpenTouchCal() {
  if (uiNavTooSoon()) {
    return;
  }
  screen = APP_TOUCH_CAL;
  calStep = 0;
  touchSuppressMs(500);
  holdOk = 0;
  autoInverts = 0;
  missFlipVotes = 0;
  uiDrawTouchCal(0, -1, -1, false, prefsTouchMapName());
}

void appNotifyPhaseIdle() {}

static uint8_t bestTouchMap(int16_t r0x, int16_t r0y, int16_t t0x, int16_t t0y, int16_t r1x, int16_t r1y,
                            int16_t t1x, int16_t t1y) {
  uint8_t best = 0;
  int32_t bestD = 0x7fffffff;
  for (uint8_t m = 0; m < 4; m++) {
    int16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    touchMapRawToScreen(m, r0x, r0y, &x0, &y0);
    touchMapRawToScreen(m, r1x, r1y, &x1, &y1);
    const int32_t d0x = static_cast<int32_t>(x0) - t0x;
    const int32_t d0y = static_cast<int32_t>(y0) - t0y;
    const int32_t d1x = static_cast<int32_t>(x1) - t1x;
    const int32_t d1y = static_cast<int32_t>(y1) - t1y;
    const int32_t d = d0x * d0x + d0y * d0y + d1x * d1x + d1y * d1y;
    if (d < bestD) {
      bestD = d;
      best = m;
    }
  }
  return best;
}

static void commitTouchMap(uint8_t map) {
  prefs().touchMap = static_cast<uint8_t>(map & 0x03);
  prefsSave();
  prefsApplyTouchMap();
}

static void finishTwoPointCal() {
  int16_t t0x = 0, t0y = 0, t1x = 0, t1y = 0;
  uiCalTargets(&t0x, &t0y, &t1x, &t1y);
  const uint8_t map = bestTouchMap(calRaw0x, calRaw0y, t0x, t0y, calRaw1x, calRaw1y, t1x, t1y);
  commitTouchMap(map);
  int16_t x1 = 0, y1 = 0;
  touchMapRawToScreen(prefs().touchMap, calRaw1x, calRaw1y, &x1, &y1);
  if (!uiCalInBlue(x1, y1) && uiCalInGreen(x1, y1)) {
    commitTouchMap(static_cast<uint8_t>(prefs().touchMap ^ 0x03));
  }
  calStep = 2;
  holdOk = 0;
  autoInverts = 0;
  Serial.printf("touch cal 2pt -> %s %s\n", prefsTouchMapName(), uiLandscape() ? "land" : "port");
  uiDrawTouchCal(2, -1, -1, false, prefsTouchMapName());
}

static void noteListMiss(int16_t x, int16_t y, int8_t hit, bool settings) {
  if (hit >= 0) {
    missFlipVotes = 0;
    return;
  }
  const int16_t w = uiWidth();
  const int16_t h = uiHeight();
  const int8_t altY = settings ? uiHitSettings(x, static_cast<int16_t>(h - 1 - y))
                               : uiHitControl(x, static_cast<int16_t>(h - 1 - y));
  const int8_t altX = settings ? uiHitSettings(static_cast<int16_t>(w - 1 - x), y)
                               : uiHitControl(static_cast<int16_t>(w - 1 - x), y);
  uint8_t bit = 0;
  if (altY >= 0) {
    bit = uiLandscape() ? 0x01 : 0x02;
  } else if (altX >= 0) {
    bit = uiLandscape() ? 0x02 : 0x01;
  } else {
    missFlipVotes = 0;
    return;
  }
  missFlipVotes++;
  if (missFlipVotes >= 2) {
    missFlipVotes = 0;
    commitTouchMap(static_cast<uint8_t>(prefs().touchMap ^ bit));
    Serial.printf("touch auto-cal bit=%u %s\n", bit, prefsTouchMapName());
    drawCurrent();
  }
}

static void applySettingsRow(uint8_t row) {
  if (row == 0) {
    appOpenTouchCal();
  } else if (row == 1) {
    const bool next = !prefs().landscape;
    prefs().landscape = next;
    prefsSave();
    touchSuppressMs(400);
    if (hooks.onLandscapeChanged) {
      hooks.onLandscapeChanged(next);
    } else {
      uiSetLandscape(next);
      drawCurrent();
    }
  } else if (row == 2) {
    prefsCycleLang();
    uiSetHint(homeHint());
    drawCurrent();
  }
}

static void handleSettingsGesture(const TouchGesture &g) {
  if (!gestTap(g)) {
    return;
  }
  const int8_t row = uiHitSettings(g.x, g.y);
  noteListMiss(g.x, g.y, row, true);
  if (row < 0) {
    return;
  }
  settingsSel = static_cast<uint8_t>(row);
  uiPatchSettings(settingsScroll, settingsSel, OFFLINE_USB != 0, touchModeName());
  applySettingsRow(settingsSel);
}

static void handleControlGesture(const TouchGesture &g) {
  if (!gestTap(g)) {
    return;
  }
  const int8_t row = uiHitControl(g.x, g.y);
  noteListMiss(g.x, g.y, row, false);
  switch (row) {
    case 0:
      prefs().dnd = !prefs().dnd;
      prefsSave();
      if (hooks.onDndChanged) {
        hooks.onDndChanged(prefs().dnd);
      }
      break;
    case 1:
      prefs().brightness = static_cast<uint8_t>((prefs().brightness + 1) % 3);
      uiSetBrightnessLevel(prefs().brightness);
      prefsSave();
      break;
    case 2:
      prefsCycleVolume();
      break;
    default:
      return;
  }
  uiPatchControl(prefs().volume, prefs().brightness, prefs().dnd);
}

static void handleHomeGesture(const TouchGesture &g) {
  if (!hooks.screenIsOn || !hooks.screenIsOn()) {
    if (hooks.toggleScreen) {
      hooks.toggleScreen();
    }
    return;
  }
  if (hooks.phaseBusy && hooks.phaseBusy()) {
    if (gestTap(g) && hooks.goIdle) {
      hooks.goIdle("touch interrupt");
    }
    return;
  }
  if (!(hooks.phaseIsIdle && hooks.phaseIsIdle())) {
    return;
  }
  if (gestVert(g)) {
    appOpenControl();
    return;
  }
  if (gestHorz(g)) {
    appOpenMenu();
    return;
  }
  if (gestTap(g) && uiHitHomeCal(g.x, g.y)) {
    appOpenTouchCal();
    return;
  }
#ifdef BW_USE_LVGL
  // Home taps are handled by LVGL (pill button only). Raw layer: swipes only.
  if (gestTap(g)) {
    return;
  }
#else
  if (gestTap(g) && uiHitTalk(g.x, g.y)) {
    Serial.printf("talk tap %d,%d\n", g.x, g.y);
    appStartTalk();
    return;
  }
  if (gestTap(g)) {
    if (hooks.onHomeTap) {
      hooks.onHomeTap();
    }
  }
#endif
}

static void handleCalGesture(const TouchGesture &g) {
  if (!gestTap(g)) {
    return;
  }
  if (calStep == 0) {
    touchLastDownRaw(&calRaw0x, &calRaw0y);
    if (calRaw0x == 0 && calRaw0y == 0) {
      touchLastRaw(&calRaw0x, &calRaw0y);
    }
    calStep = 1;
    uiDrawTouchCal(1, -1, -1, false, prefsTouchMapName());
    return;
  }
  if (calStep == 1) {
    touchLastDownRaw(&calRaw1x, &calRaw1y);
    if (calRaw1x == 0 && calRaw1y == 0) {
      touchLastRaw(&calRaw1x, &calRaw1y);
    }
    finishTwoPointCal();
    return;
  }
  UiCalGeom box{};
  uiCalGeom(&box);
  if (g.y >= box.doneY) {
    appOpenSettings();
  }
}

void appHandleGesture(const TouchGesture &g) {
  appNoteActivity(millis());
#ifdef BW_USE_LVGL
  // LVGL owns taps on overlay screens; raw layer only drives home swipes.
  if (screen == APP_HOME) {
    handleHomeGesture(g);
    return;
  }
  if (screen == APP_TOUCH_CAL) {
    handleCalGesture(g);
  }
#else
  switch (screen) {
    case APP_HOME:
      handleHomeGesture(g);
      break;
    case APP_SETTINGS:
      handleSettingsGesture(g);
      break;
    case APP_CONTROL:
      handleControlGesture(g);
      break;
    case APP_TOUCH_CAL:
      handleCalGesture(g);
      break;
    default:
      break;
  }
#endif
}

void appTick(unsigned long now) {
  if (talkQueued) {
    runQueuedTalk();
  }
  if (AUTO_SLEEP_MS > 0 && screen == APP_HOME && hooks.screenIsOn && hooks.screenIsOn() &&
      hooks.blankScreen) {
    if (lastActivityAt > 0 && (now - lastActivityAt) >= AUTO_SLEEP_MS) {
      if (hooks.phaseIsIdle && hooks.phaseIsIdle()) {
        Serial.println("Auto sleep");
        hooks.blankScreen();
        lastActivityAt = now;
      }
    }
  }

  if (screen != APP_TOUCH_CAL || calStep != 2) {
    return;
  }

  int16_t x = 0;
  int16_t y = 0;
  bool down = false;
  if (!touchSample(&x, &y, &down)) {
    return;
  }
  uiDrawTouchCal(2, x, y, down, prefsTouchMapName());
  if (!down) {
    holdOk = 0;
    return;
  }
  if (uiCalInGreen(x, y)) {
    holdOk++;
    if (holdOk >= 12) {
      Serial.println("touch cal hold OK");
      appOpenSettings();
    }
    return;
  }
  if (uiCalInBlue(x, y) && autoInverts < 2) {
    autoInverts++;
    holdOk = 0;
    commitTouchMap(static_cast<uint8_t>(prefs().touchMap ^ 0x03));
    Serial.println("touch auto-cal invert 180");
    uiDrawTouchCal(2, -1, -1, false, prefsTouchMapName());
  }
}

bool appTalkBusy() {
  if (talkBusy && talkBusyAt && millis() - talkBusyAt > 60000UL) {
    finishTalk();
  }
  return talkBusy || talkQueued || millis() < talkCoolUntil;
}

static uint8_t rmsLevel(uint16_t rms) {
  if (rms < 40) {
    return 0;
  }
  if (rms > 4000) {
    return 100;
  }
  return static_cast<uint8_t>((rms - 40) * 100 / 3960);
}

static void paintRec(unsigned left, uint8_t level) {
  char line[24];
  snprintf(line, sizeof(line), "REC %us", left);
  uiShow(EMO_LISTEN, line);
  uiPatchRecLevel(level);
  uiFlush();
}

static void onMicProgress(uint16_t chunkRms, size_t got, size_t total) {
  const unsigned left = total > got ? static_cast<unsigned>((total - got + 15999) / 16000) : 0;
  char line[24];
  snprintf(line, sizeof(line), "REC %us", left);
  uiSetMicLevel(chunkRms);
  uiPatchSubtitle(line);
  uiPatchRecLevel(rmsLevel(chunkRms));
  uiFlush();
}

static void showTalkBlocked() {
  restoreTalkIdleUi();
  if (!netWifiOk()) {
    uiPatchSubtitle(tr("没网 · 滑动仍可用", "Offline · swipes OK"));
  } else {
    uiPatchSubtitle(tr("云端不可用", "Cloud unavailable"));
  }
}

static bool recordAndUpload(size_t samples, const char *listenLine, uint16_t *rmsOut) {
  if (!netCanTalk()) {
    showTalkBlocked();
    return false;
  }

  int16_t *pcm = static_cast<int16_t *>(heap_caps_malloc(samples * sizeof(int16_t),
                                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!pcm) {
    pcm = static_cast<int16_t *>(malloc(samples * sizeof(int16_t)));
  }
  if (!pcm) {
    uiShow(EMO_OFFLINE, "MIC oom");
    return false;
  }
  appGoHome();
  paintRec((samples + 15999) / 16000, 0);
  netPublishFace("listen", listenLine);
  const uint16_t rms = micRecordPcm(pcm, samples, onMicProgress);
  if (rmsOut) {
    *rmsOut = rms;
  }
  uiSetRecord(false, 0, nullptr);
  char line[40];
  snprintf(line, sizeof(line), "rms=%u", rms);
  uiShow(EMO_THINK, line);
  uiTick();
  const bool ok = netUploadWav(pcm, samples);
  free(pcm);
  return ok;
}

static void restoreTalkIdleUi() {
  uiSetRecord(false, 0, nullptr);
  uiResumeHome();
}

static void finishTalk() {
  talkBusy = false;
  talkQueued = false;
  talkCoolUntil = millis() + 400UL;
  micAbortRequest();
  micEnd();
  uiSetRecord(false, 0, nullptr);
  touchSuppressMs(200);
}

static void cancelTalk() {
  talkQueued = false;
  talkBusy = false;
  talkCoolUntil = 0;
  micAbortRequest();
  micEnd();
  restoreTalkIdleUi();
  touchSuppressMs(200);
  Serial.println("talk cancel");
}

static void queueTalk(bool micOnly) {
  if (talkBusy || talkQueued || millis() < talkCoolUntil || uiNavTooSoon(500)) {
    return;
  }
  talkQueued = true;
  talkMicOnly = micOnly;
  talkBusy = true;
  talkBusyAt = millis();
  Serial.println(micOnly ? "talk queue mic" : "talk queue");
}

static void runQueuedTalk() {
  talkQueued = false;
  talkBusy = true;
  talkBusyAt = millis();
  if (!netCanTalk()) {
    showTalkBlocked();
    finishTalk();
    return;
  }
  uint16_t rms = 0;
  char line[40];
  if (talkMicOnly) {
    if (recordAndUpload(16000 * 10, tr("正在听…", "Listening..."), &rms)) {
      snprintf(line, sizeof(line), "MIC ok rms=%u", rms);
      uiShow(EMO_SPEAK, line);
      netPublishFace("speak", line);
    } else {
      snprintf(line, sizeof(line), "MIC fail rms=%u", rms);
      uiShow(EMO_OFFLINE, line);
    }
    Serial.println(line);
    finishTalk();
    return;
  }
  if (!recordAndUpload(16000 * 10, tr("正在听…", "Listening..."), &rms)) {
    uiShow(EMO_OFFLINE, "MIC fail");
    finishTalk();
    return;
  }
  if (rms < 80) {
    uiShow(EMO_QUIET, tr("没听清", "too quiet"));
    netPublishFace("quiet", tr("没听清", "too quiet"));
    finishTalk();
    return;
  }
  uiShow(EMO_THINK, tr("思考中…", "Thinking..."));
  uiFlush();
  CloudReply reply = netTurn(tr("手表在跟你说话", "watch is talking"));
  if (!reply.ok) {
    uiShow(EMO_SPEAK, "MIC ok");
    netPublishFace("speak", "MIC ok");
    finishTalk();
    return;
  }
  uiShow(reply.emotion, reply.text);
  netPublishFace("speak", reply.text);
  finishTalk();
}

void appRunMicTx() {
  if (appTalkBusy()) {
    cancelTalk();
    return;
  }
  if (!netCanTalk()) {
    showTalkBlocked();
    return;
  }
  queueTalk(true);
}

void appStartTalk() {
  if (appTalkBusy()) {
    cancelTalk();
    return;
  }
  restoreTalkIdleUi();
#if !VOICE_FEATURES
  if (hooks.onHomeTap) {
    hooks.onHomeTap();
  }
  return;
#endif
  if (!netCanTalk()) {
    showTalkBlocked();
    return;
  }
  queueTalk(false);
}
