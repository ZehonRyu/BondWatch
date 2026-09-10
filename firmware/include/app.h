#pragma once

#include "touch.h"
#include <stdint.h>

enum AppScreen : uint8_t {
  APP_HOME = 0,
  APP_CONTROL,
  APP_SETTINGS,
  APP_TOUCH_CAL,
};

struct AppHooks {
  void (*startTalk)(const char *why);
  void (*goIdle)(const char *why);
  void (*onDndChanged)(bool on);
  void (*onLandscapeChanged)(bool on);
  void (*onHomeTap)();
  void (*toggleScreen)();
  void (*blankScreen)();
  bool (*phaseIsIdle)();
  bool (*phaseBusy)();
  bool (*screenIsOn)();
};

void appBegin(const AppHooks *hooks);
AppScreen appScreen();
void appGoHome();
void appOpenSettings();
void appOpenControl();
void appOpenTouchCal();
void appHandleGesture(const TouchGesture &g);
void appTick(unsigned long now);
void appNotifyPhaseIdle();
void appRedraw();
void appNoteActivity(unsigned long now);
