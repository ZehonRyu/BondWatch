#pragma once

#include <stdint.h>

enum BrightnessLevel : uint8_t { BRIGHT_LOW = 0, BRIGHT_MID = 1, BRIGHT_HIGH = 2 };
enum VadLevel : uint8_t { VAD_OFF = 0, VAD_NORMAL = 1, VAD_SENSITIVE = 2 };
enum VolumeLevel : uint8_t { VOL_MUTE = 0, VOL_LOW = 1, VOL_MID = 2, VOL_HIGH = 3 };
enum Lang : uint8_t { LANG_ZH = 0, LANG_EN = 1 };

struct PrefsState {
  uint8_t brightness; // BrightnessLevel
  uint8_t volume;     // VolumeLevel — 软件音量，喇叭接上后生效
  bool dnd;
  bool landscape;
  uint8_t vad; // VadLevel
  uint8_t lang; // Lang — UI language, default Chinese
  uint8_t touchMap; // bit0=flipX bit1=flipY (bit2 leftover, ignored)
  uint32_t alarmRemainSec; // 0 = none; persisted countdown
};

void prefsBegin();
PrefsState &prefs();
void prefsSave();
void prefsPrint();
void prefsApplyTouchMap(); // push transform into touch driver

uint16_t prefsVadThreshold(); // mic RMS threshold for VAD
uint8_t prefsBrightnessDuty(); // 0..255 PWM
const char *prefsBrightnessName();
const char *prefsVolumeName();
const char *prefsVadName();
const char *prefsLangName();
const char *prefsTouchMapName();
void prefsCycleTouchMap();
void prefsInvertTouchMap(); // XOR both flips — 180° / centrosymmetric fix
void prefsCycleVolume(); // mute -> low -> mid -> high -> mute
void prefsCycleLang();   // zh <-> en

// Alarm helpers (RAM deadline derived from remain).
void prefsArmAlarmMinutes(uint32_t minutes);
void prefsArmAlarmSeconds(uint32_t seconds);
void prefsCancelAlarm();
bool prefsAlarmArmed();
unsigned long prefsAlarmDeadline(); // millis(), 0 if none
void prefsAlarmTick(unsigned long now); // refresh NVS remain
bool prefsAlarmDue(unsigned long now);
