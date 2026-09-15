#pragma once

#include <Arduino.h>

enum Emotion : uint8_t {
  EMO_IDLE,
  EMO_LISTEN,
  EMO_THINK,
  EMO_SPEAK,
  EMO_QUIET,
  EMO_SILENT,
  EMO_ALARM,
  EMO_OFFLINE,
};

void uiBegin();
void uiSetBacklight(bool on);
void uiSetBrightnessLevel(uint8_t level);
void uiSetLandscape(bool landscape);
bool uiLandscape();
void uiSetFlags(bool wifi, bool lte, bool dnd);
void uiSetMicLevel(uint16_t rms);
void uiSetRecord(bool on, uint8_t level, const char *status);
void uiPatchRecLevel(uint8_t level);
float uiMicSmooth();
void uiSetHint(const char *hint);
void uiSetPair(const char *line);
void uiSetAlarmBadge(const char *badge);
void uiShow(Emotion emotion, const char *subtitle);
void uiResumeHome();
void uiFillClock(char *buf, size_t n);
bool uiTimeSynced();
void uiTick();
void uiFlush();
void uiBlank();

int16_t uiWidth();
int16_t uiHeight();

void uiDrawMenu();
void uiDrawLteTest();
void uiPatchLteTest(const char *status, const char *id, const char *sim, const char *csq, const char *hint,
                    uint32_t statusColor);
void uiDrawSettings(uint8_t scroll, uint8_t selected, bool offline, const char *touchMode);
void uiDrawControl(uint8_t volume, uint8_t brightness, bool dnd);
int8_t uiHitBand(int16_t y, int16_t top, int16_t rowH, uint8_t count);
int8_t uiHitControl(int16_t x, int16_t y);
int8_t uiHitSettings(int16_t x, int16_t y);
bool uiHitHomeCal(int16_t x, int16_t y);
bool uiHitTalk(int16_t x, int16_t y);

#define BW_VERSION "C24"

#define UI_CTRL_TOP 56
#define UI_CTRL_ROW 80
#define UI_CTRL_N 3
#define UI_SET_TOP 50
#define UI_SET_ROW 78
#define UI_SET_N 3
#define UI_LAND_TILE_Y 40

struct UiCalGeom {
  int16_t gx, gy, gw, gh;
  int16_t bx, by, bw, bh;
  int16_t doneY;
};

void uiCalGeom(UiCalGeom *out);
void uiCalTargets(int16_t *t0x, int16_t *t0y, int16_t *t1x, int16_t *t1y);
bool uiCalInGreen(int16_t x, int16_t y);
bool uiCalInBlue(int16_t x, int16_t y);

void uiPatchControl(uint8_t volume, uint8_t brightness, bool dnd);
void uiPatchSettings(uint8_t scroll, uint8_t selected, bool offline, const char *touchMode);
void uiPatchSubtitle(const char *subtitle);
void uiDrawTouchCal(uint8_t step, int16_t x, int16_t y, bool down, const char *mapName);
void uiScreenshotToSerial();
