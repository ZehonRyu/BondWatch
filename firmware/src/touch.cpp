#include "touch.h"
#include "pins.h"

#include <Arduino.h>
#include <Wire.h>

static const uint8_t CST816_ADDR = 0x15;
static const uint8_t REG_GESTURE = 0x01;
static const uint8_t REG_CHIP_ID = 0xA7;
static const uint8_t REG_IRQ_CTL = 0xFA;
static const uint8_t REG_AUTO_SLEEP = 0xF9;
static const uint8_t REG_DIS_AUTOSLEEP = 0xFE;

static bool i2cOk = false;
static bool flipX = false;
static bool flipY = true; // Waveshare 2.0 default until prefs loads
static uint8_t dispRot = 0; // 0 portrait 240x320, 1 landscape 320x240
static int lastInt = HIGH;
static int16_t lastRawX = 0;
static int16_t lastRawY = 0;
static int16_t downRawX = 0;
static int16_t downRawY = 0;

static bool fingerDown = false;
static int16_t downX = 0;
static int16_t downY = 0;
static int16_t lastX = 0;
static int16_t lastY = 0;
static unsigned long downAt = 0;
static unsigned long lastTapAt = 0;
static unsigned long suppressUntil = 0;
static bool longFired = false;
static bool moved = false;
static bool debugTouch = false;
static uint8_t lastChipGest = 0;
static bool emitGestures = true;

static TouchGesture pending{};
static bool hasPending = false;

// Watch face is small; 40px is a real swipe, below that stays a tap.
static const int MOVE_PX = 18;
static const int SWIPE_PX = 44;
static const unsigned LONG_MS = 750;
static const unsigned MIN_TAP_MS = 55;
static const unsigned MAX_TAP_MS = 520;
static const unsigned TAP_EMIT_GAP_MS = 400;
static unsigned long lastTapEmitAt = 0;

static const char *gestName(TouchGestureType t) {
  switch (t) {
    case TG_TAP:
      return "TAP";
    case TG_DOUBLE:
      return "DOUBLE";
    case TG_LONG:
      return "LONG";
    case TG_SWIPE_UP:
      return "UP";
    case TG_SWIPE_DOWN:
      return "DOWN";
    case TG_SWIPE_LEFT:
      return "LEFT";
    case TG_SWIPE_RIGHT:
      return "RIGHT";
    default:
      return "?";
  }
}

static bool i2cWrite8(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(CST816_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool i2cRead(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(CST816_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const size_t n = Wire.requestFrom(CST816_ADDR, static_cast<uint8_t>(len));
  if (n != len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    buf[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

static void applyMap(uint8_t map, int16_t rawX, int16_t rawY, int16_t *sx, int16_t *sy) {
  if (rawX < 0) {
    rawX = 0;
  }
  if (rawY < 0) {
    rawY = 0;
  }
  if (rawX > 239) {
    rawX = 239;
  }
  if (rawY > 319) {
    rawY = 319;
  }

  int16_t x = rawX;
  int16_t y = rawY;
  if (map & 1) {
    x = static_cast<int16_t>(239 - x);
  }
  if (map & 2) {
    y = static_cast<int16_t>(319 - y);
  }
  if (dispRot == 1) {
    const int16_t nx = x;
    const int16_t ny = y;
    x = ny;
    y = static_cast<int16_t>(239 - nx);
  }
  *sx = x;
  *sy = y;
}

static void mapPoint(int16_t rawX, int16_t rawY, int16_t *sx, int16_t *sy) {
  uint8_t map = 0;
  if (flipX) {
    map |= 1;
  }
  if (flipY) {
    map |= 2;
  }
  applyMap(map, rawX, rawY, sx, sy);
}

void touchMapRawToScreen(uint8_t map, int16_t rawX, int16_t rawY, int16_t *sx, int16_t *sy) {
  applyMap(static_cast<uint8_t>(map & 0x03), rawX, rawY, sx, sy);
}

// Chip IDs are in native 240x320: 1=up 2=down 3=left 4=right.
static TouchGestureType mapChipGesture(uint8_t id) {
  TouchGestureType t = TG_NONE;
  if (id == 1) {
    t = TG_SWIPE_UP;
  } else if (id == 2) {
    t = TG_SWIPE_DOWN;
  } else if (id == 3) {
    t = TG_SWIPE_LEFT;
  } else if (id == 4) {
    t = TG_SWIPE_RIGHT;
  } else {
    return TG_NONE;
  }
  if (flipX) {
    if (t == TG_SWIPE_LEFT) {
      t = TG_SWIPE_RIGHT;
    } else if (t == TG_SWIPE_RIGHT) {
      t = TG_SWIPE_LEFT;
    }
  }
  if (flipY) {
    if (t == TG_SWIPE_UP) {
      t = TG_SWIPE_DOWN;
    } else if (t == TG_SWIPE_DOWN) {
      t = TG_SWIPE_UP;
    }
  }
  if (dispRot == 1) {
    switch (t) {
      case TG_SWIPE_UP:
        return TG_SWIPE_LEFT;
      case TG_SWIPE_DOWN:
        return TG_SWIPE_RIGHT;
      case TG_SWIPE_LEFT:
        return TG_SWIPE_DOWN;
      case TG_SWIPE_RIGHT:
        return TG_SWIPE_UP;
      default:
        break;
    }
  }
  return t;
}

static void queueGesture(TouchGestureType type, int16_t x, int16_t y) {
  if (!emitGestures) {
    return;
  }
  const unsigned long now = millis();
  if (type == TG_TAP || type == TG_DOUBLE) {
    if (now - lastTapEmitAt < TAP_EMIT_GAP_MS) {
      return;
    }
    lastTapEmitAt = now;
    type = TG_TAP;
  }
  pending.type = type;
  pending.x = x;
  pending.y = y;
  hasPending = true;
  Serial.printf("touch %s @%d,%d\n", gestName(type), x, y);
}

static bool readPoint(int16_t *sx, int16_t *sy, bool *down) {
  uint8_t buf[7];
  if (!i2cRead(REG_GESTURE, buf, 7)) {
    return false;
  }
  lastChipGest = buf[0];
  const uint8_t fingerRaw = buf[1];
  const uint8_t fingers = fingerRaw & 0x0F;
  const int16_t rawX = static_cast<int16_t>(((buf[2] & 0x0F) << 8) | buf[3]);
  const int16_t rawY = static_cast<int16_t>(((buf[4] & 0x0F) << 8) | buf[5]);
  lastRawX = rawX;
  lastRawY = rawY;
  mapPoint(rawX, rawY, sx, sy);
  // 0xFF = CST816 auto-sleep (same meaning as OV-Watch FingerNum).
  if (fingerRaw == 0xFF) {
    i2cWrite8(REG_DIS_AUTOSLEEP, 0x01);
    *down = false;
  } else {
    *down = fingers > 0 && fingers < 3;
  }
  if (debugTouch) {
    Serial.printf("tp raw=%d,%d map=%d,%d f=%u g=0x%02X XY%c%c\n", rawX, rawY, *sx, *sy, fingers,
                  lastChipGest, flipX ? 'X' : '-', flipY ? 'Y' : '-');
  }
  return true;
}

void touchBegin() {
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(10);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(100);
  Wire.begin(PIN_TP_SDA, PIN_TP_SCL);
  Wire.setClock(400000);

  uint8_t id = 0;
  i2cOk = i2cRead(REG_CHIP_ID, &id, 1);
  if (!i2cOk) {
    delay(30);
    Wire.setClock(100000);
    i2cOk = i2cRead(REG_CHIP_ID, &id, 1);
    if (i2cOk) {
      Serial.println("touch: I2C fell back to 100kHz");
    }
  }
  if (i2cOk) {
    // Same chip as OV-Watch: poll I2C, do not require INT. Keep it from sleeping.
    i2cWrite8(REG_DIS_AUTOSLEEP, 0x01);
    i2cWrite8(REG_AUTO_SLEEP, 0);
    i2cWrite8(REG_IRQ_CTL, 0x70); // EnTouch | EnChange | EnMotion
    Serial.printf("touch: CST816 ok id=0x%02X  native 240x320\n", id);
  } else {
    Serial.println("touch: INT-only fallback — check TP wiring");
  }
  lastInt = digitalRead(PIN_TP_INT);
}

void touchSetRotation(uint8_t rot) {
  dispRot = (rot == 1) ? 1 : 0;
}

void touchSetTransform(bool fx, bool fy, bool swap) {
  (void)swap;
  flipX = fx;
  flipY = fy;
  Serial.printf("touch transform flipX=%d flipY=%d rot=%u\n", fx ? 1 : 0, fy ? 1 : 0, dispRot);
}

void touchLastRaw(int16_t *rawX, int16_t *rawY) {
  if (rawX) {
    *rawX = lastRawX;
  }
  if (rawY) {
    *rawY = lastRawY;
  }
}

void touchLastDownRaw(int16_t *rawX, int16_t *rawY) {
  if (rawX) {
    *rawX = downRawX;
  }
  if (rawY) {
    *rawY = downRawY;
  }
}

bool touchHasCoords() {
  return i2cOk;
}

const char *touchModeName() {
  return i2cOk ? "CST816" : "INT";
}

bool touchSuppressed() {
  return millis() < suppressUntil;
}

void touchSuppressMs(unsigned ms) {
  suppressUntil = millis() + ms;
  hasPending = false;
  fingerDown = false;
  longFired = false;
  moved = false;
}

bool touchFingerDown() {
  return fingerDown;
}

void touchSetDebug(bool on) {
  debugTouch = on;
  Serial.println(on ? "touch debug ON" : "touch debug OFF");
}

bool touchSample(int16_t *x, int16_t *y, bool *down) {
  if (!i2cOk) {
    return false;
  }
  // Always read the chip. INT is optional — OV-Watch also polls I2C.
  return readPoint(x, y, down);
}

void touchPointer(int16_t *x, int16_t *y, bool *down) {
  if (x) {
    *x = lastX;
  }
  if (y) {
    *y = lastY;
  }
  if (down) {
    *down = fingerDown;
  }
}

static void finishTap(int16_t x, int16_t y, unsigned long now) {
  (void)x;
  (void)y;
  lastTapAt = now;
  queueGesture(TG_TAP, x, y);
}

static void endFinger(unsigned long now) {
  fingerDown = false;
  if (longFired) {
    lastChipGest = 0;
    return;
  }
  const int dx = lastX - downX;
  const int dy = lastY - downY;
  const int adx = abs(dx);
  const int ady = abs(dy);
  if (adx > SWIPE_PX || ady > SWIPE_PX) {
    if (ady >= adx) {
      queueGesture(dy < 0 ? TG_SWIPE_UP : TG_SWIPE_DOWN, lastX, lastY);
    } else {
      queueGesture(dx < 0 ? TG_SWIPE_LEFT : TG_SWIPE_RIGHT, lastX, lastY);
    }
  } else {
    const unsigned held = now - downAt;
    if (held < MIN_TAP_MS || held > MAX_TAP_MS) {
      lastChipGest = 0;
      return;
    }
    const TouchGestureType chip = mapChipGesture(lastChipGest);
    if (chip != TG_NONE) {
      queueGesture(chip, lastX, lastY);
    } else {
      finishTap(downX, downY, now);
    }
  }
  lastChipGest = 0;
}

static void pollI2c(unsigned long now) {
  // CST816 INT is active-low while touched. Also poll without INT — a missing
  // TP_INT wire used to make the whole panel look dead even when I2C worked.
  const int irq = digitalRead(PIN_TP_INT);
  lastInt = irq;
  static unsigned long lastIdlePoll = 0;
  if (!fingerDown && irq == HIGH) {
    if (now - lastIdlePoll < 12) {
      return;
    }
  }
  lastIdlePoll = now;

  int16_t sx = 0;
  int16_t sy = 0;
  bool down = false;
  if (!readPoint(&sx, &sy, &down)) {
    // Lift often makes the next I2C read fail while INT is already high.
    // Treat that as release so taps are not silently dropped.
    if (fingerDown && irq == HIGH) {
      endFinger(now);
    }
    return;
  }

  if (down && !fingerDown) {
    fingerDown = true;
    downAt = now;
    downX = sx;
    downY = sy;
    downRawX = lastRawX;
    downRawY = lastRawY;
    lastX = sx;
    lastY = sy;
    longFired = false;
    moved = false;
  } else if (down && fingerDown) {
    lastX = sx;
    lastY = sy;
    if (abs(sx - downX) > MOVE_PX || abs(sy - downY) > MOVE_PX) {
      moved = true;
    }
    if (!longFired && !moved && now - downAt >= LONG_MS) {
      longFired = true;
      queueGesture(TG_LONG, sx, sy);
    }
  } else if (!down && fingerDown) {
    endFinger(now);
  } else if (!down && !fingerDown) {
    const TouchGestureType chip = mapChipGesture(lastChipGest);
    if (chip != TG_NONE) {
      queueGesture(chip, sx, sy);
      lastChipGest = 0;
    }
  }
}

static void pollIntOnly(unsigned long now) {
  const int level = digitalRead(PIN_TP_INT);
  const bool edge = lastInt == HIGH && level == LOW;
  lastInt = level;
  if (!edge) {
    return;
  }
  const int16_t cx = (dispRot == 1) ? 160 : 120;
  const int16_t cy = (dispRot == 1) ? 120 : 160;
  finishTap(cx, cy, now);
}

bool touchPollGesture(TouchGesture *out) {
  const unsigned long now = millis();
  // Keep sampling during boot suppress so the first real swipe is not lost,
  // but do not emit those ghost contacts.
  emitGestures = !touchSuppressed();
  if (i2cOk) {
    pollI2c(now);
  } else {
    pollIntOnly(now);
  }
  if (!emitGestures) {
    hasPending = false;
    return false;
  }
  if (!hasPending || !out) {
    return false;
  }
  *out = pending;
  hasPending = false;
  return true;
}

bool touchTapped() {
  TouchGesture g{};
  if (!touchPollGesture(&g)) {
    return false;
  }
  return g.type == TG_TAP || g.type == TG_DOUBLE;
}
