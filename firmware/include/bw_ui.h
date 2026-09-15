#pragma once

#include <stdint.h>

void bwUiStart();
void bwUiTick();
void bwUiFlush();
void bwUiShowHome(const char *subtitle, uint32_t faceHex);
void bwUiSetSubtitle(const char *subtitle);
void bwUiSetPair(const char *line);
void bwUiSetRecord(bool on, uint8_t level, const char *status);
void bwUiPatchRecLevel(uint8_t level);
void bwUiShowControl();
void bwUiShowSettings();
void bwUiShowCal();
void bwUiShowMenu();
void bwUiShowLte();
void bwUiPatchLte(const char *status, const char *id, const char *sim, const char *csq, const char *hint,
                  uint32_t statusColor);
void bwUiPatchClock();
void bwUiPatchCal(uint8_t step, int16_t x, int16_t y, bool down, const char *mapName);
void bwUiPatchControl(uint8_t volume, uint8_t brightness, bool dnd);
void bwUiPatchSettings(const char *orient, const char *lang, const char *touchMode);
bool bwUiHitTalk(int16_t x, int16_t y);
