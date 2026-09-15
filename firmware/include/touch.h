#pragma once

#include <stdint.h>

enum TouchGestureType : uint8_t {
  TG_NONE = 0,
  TG_TAP,
  TG_DOUBLE,
  TG_LONG,
  TG_SWIPE_UP,
  TG_SWIPE_DOWN,
  TG_SWIPE_LEFT,
  TG_SWIPE_RIGHT,
};

struct TouchGesture {
  TouchGestureType type;
  int16_t x;
  int16_t y;
};

void touchBegin();
void touchSetRotation(uint8_t rot); // 0 = 240x320, 1 = 320x240
void touchSetTransform(bool flipX, bool flipY, bool swapXY); // swapXY ignored
// Map a CST816 raw sample through map bits + current rotation into screen pixels.
void touchMapRawToScreen(uint8_t map, int16_t rawX, int16_t rawY, int16_t *sx, int16_t *sy);
bool touchHasCoords();              // true when CST816 I2C works
const char *touchModeName();
bool touchSuppressed();             // true shortly after a gesture (mute VAD)
void touchSuppressMs(unsigned ms);  // ignore new gestures for a while (boot settle)
bool touchFingerDown();             // true while a finger contact is active
void touchSetDebug(bool on);
void touchLastRaw(int16_t *rawX, int16_t *rawY); // last CST816 sample, before flip
void touchLastDownRaw(int16_t *rawX, int16_t *rawY); // raw at finger-down (for cal)
// Live sample for calibration UI (works while finger down).
bool touchSample(int16_t *x, int16_t *y, bool *down);
// Last mapped pointer from the I2C poll (no extra bus read). For LVGL indev.
void touchPointer(int16_t *x, int16_t *y, bool *down);
// Non-blocking: returns true when a gesture is ready.
bool touchPollGesture(TouchGesture *out);
// Compat: true on a completed single tap (or INT-only tap).
bool touchTapped();
