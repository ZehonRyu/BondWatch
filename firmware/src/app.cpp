#include "app.h"

#include "prefs.h"
#include "ui.h"
#include "secrets.h"
#include "lang.h"

#include <Arduino.h>
#include <stdio.h>

static AppHooks hooks{};
static AppScreen screen = APP_HOME;
static uint8_t settingsScroll = 0;
static uint8_t settingsSel = 0;
static unsigned long lastActivityAt = 0;
static uint8_t calStep = 0;

static int16_t calRaw0x = 0, calRaw0y = 0;
static int16_t calRaw1x = 0, calRaw1y = 0;
static uint8_t holdOk = 0;
static uint8_t autoInverts = 0;
static uint8_t missFlipVotes = 0;

static const unsigned long AUTO_SLEEP_MS =
#if OFFLINE_USB
    0UL
#else
    120000UL
#endif
    ;

static bool gestBack(const TouchGesture &g) { return g.type == TG_SWIPE_DOWN; }

static bool gestTap(const TouchGesture &g) { return g.type == TG_TAP || g.type == TG_DOUBLE; }

static const char *homeHint() { return tr("上滑控制", "swipe up"); }

static void drawCurrent() {
  switch (screen) {
    case APP_CONTROL:
      uiDrawControl(prefs().volume, prefs().brightness, prefs().dnd);
      break;
    case APP_SETTINGS:
      uiDrawSettings(settingsScroll, settingsSel, OFFLINE_USB != 0, touchModeName());
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
  screen = APP_HOME;
  if (fromOverlay && hooks.goIdle) {
    hooks.goIdle("home");
  }
}

void appOpenSettings() {
  screen = APP_SETTINGS;
  settingsScroll = 0;
  settingsSel = 0;
  drawCurrent();
}

void appOpenControl() {
  screen = APP_CONTROL;
  drawCurrent();
}

void appOpenTouchCal() {
  screen = APP_TOUCH_CAL;
  calStep = 0;
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
    prefsCycleLang();
    uiSetHint(homeHint());
    drawCurrent();
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
    appOpenTouchCal();
  }
}

static void handleSettingsGesture(const TouchGesture &g) {
  if (gestBack(g)) {
    appGoHome();
    return;
  }
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
  if (gestBack(g)) {
    appGoHome();
    return;
  }
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
  if (g.type == TG_SWIPE_UP) {
    appOpenControl();
    return;
  }
  if (g.type == TG_SWIPE_LEFT) {
    appOpenSettings();
    return;
  }
  if (gestTap(g)) {
#if VOICE_FEATURES
    if (hooks.startTalk) {
      hooks.startTalk("touch tap");
    }
#else
    if (hooks.onHomeTap) {
      hooks.onHomeTap();
    }
#endif
  }
}

static void handleCalGesture(const TouchGesture &g) {
  if (gestBack(g)) {
    appOpenSettings();
    return;
  }
  if (!gestTap(g)) {
    return;
  }
  if (calStep == 0) {
    touchLastDownRaw(&calRaw0x, &calRaw0y);
    calStep = 1;
    uiDrawTouchCal(1, -1, -1, false, prefsTouchMapName());
    return;
  }
  if (calStep == 1) {
    touchLastDownRaw(&calRaw1x, &calRaw1y);
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
  switch (screen) {
    case APP_SETTINGS:
      handleSettingsGesture(g);
      break;
    case APP_CONTROL:
      handleControlGesture(g);
      break;
    case APP_TOUCH_CAL:
      handleCalGesture(g);
      break;
    case APP_HOME:
    default:
      handleHomeGesture(g);
      break;
  }
}

void appTick(unsigned long now) {
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
