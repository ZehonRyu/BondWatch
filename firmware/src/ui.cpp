#include "ui.h"
#include "app.h"
#include "pins.h"
#include "prefs.h"
#include "lang.h"
#include "secrets.h"
#include "touch.h"
#include "lv_port.h"
#include "bw_ui.h"
#include "net.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>

static SPIClass lcdSpi(FSPI);
static Adafruit_ST7789 tft(&lcdSpi, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);
static Adafruit_GFX *gfx = &tft;
static U8G2_FOR_ADAFRUIT_GFX u8f;
static GFXcanvas16 *shotCanvas = nullptr;

struct Face {
  uint16_t bg;
  uint16_t fill;
};

static Emotion lastEmotion = EMO_IDLE;
static uint16_t lastBg = 0x0000;
static bool landscape = false;
static bool flagWifi = true;
static bool flagLte = false;
static bool flagDnd = false;
static bool backlightOn = true;
static uint8_t brightLevel = BRIGHT_MID;
static char lastLine[48] = "";
static char hintLine[40] = "";
static char alarmBadge[16] = "";
static uint16_t micRms = 0;
static uint16_t micRmsDrawn = 0xFFFF;
static float micFloor = 80.0f;
static float micSmooth = 0.0f;
static int16_t micFillDrawn = -1;

static Face faceOf(Emotion emotion) {
  switch (emotion) {
    case EMO_LISTEN:
      return {0x0120, 0x07E8};
    case EMO_THINK:
      return {0x1008, 0xC81F};
    case EMO_SPEAK:
      return {0x2080, 0xFD20};
    case EMO_QUIET:
      return {0x0841, 0x7BEF};
    case EMO_SILENT:
      return {0x0000, 0x6B6D};
    case EMO_ALARM:
      return {0x4800, 0xF800};
    case EMO_OFFLINE:
      return {0x0841, 0x8410};
    case EMO_IDLE:
    default:
      return {0x0000, 0x5E9D};
  }
}

static const char *emotionLabel(Emotion emotion) {
  switch (emotion) {
    case EMO_LISTEN:
      return tr("聆听", "listen");
    case EMO_THINK:
      return tr("思考", "think");
    case EMO_SPEAK:
      return tr("说话", "speak");
    case EMO_QUIET:
      return tr("轻声", "quiet");
    case EMO_SILENT:
      return tr("勿扰", "silent");
    case EMO_ALARM:
      return tr("闹钟", "alarm");
    case EMO_OFFLINE:
      return tr("离线", "offline");
    case EMO_IDLE:
    default:
      return tr("待机", "idle");
  }
}

static bool isTalking(Emotion emotion) {
  return emotion == EMO_LISTEN || emotion == EMO_SPEAK || emotion == EMO_ALARM;
}

static const int BL_LEDC_CH = 0;

// Opaque UTF-8 text (Chinese + English). y is top of text box.
static void uiText(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, bool title = false) {
  if (!s) {
    return;
  }
  u8f.setFontMode(0);
  u8f.setForegroundColor(fg);
  u8f.setBackgroundColor(bg);
  u8f.setFont(u8g2_font_wqy14_t_gb2312b);
  (void)title;
  u8f.setCursor(x, y + 14);
  u8f.print(s);
}

static void uiClock(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, const uint8_t *font,
                   int16_t cap) {
  u8f.setFontMode(0);
  u8f.setFont(font);
  u8f.setForegroundColor(fg);
  u8f.setBackgroundColor(bg);
  u8f.setCursor(x, y + cap);
  u8f.print(s);
}

static int16_t textW(const char *s) {
  u8f.setFontMode(0);
  u8f.setFont(u8g2_font_wqy14_t_gb2312b);
  return static_cast<int16_t>(u8f.getUTF8Width(s ? s : ""));
}

static uint16_t darken(uint16_t c, uint8_t n) {
  uint8_t r = static_cast<uint8_t>((c >> 11) & 0x1F);
  uint8_t g = static_cast<uint8_t>((c >> 5) & 0x3F);
  uint8_t b = static_cast<uint8_t>(c & 0x1F);
  r = r > n ? static_cast<uint8_t>(r - n) : 0;
  g = g > (n * 2) ? static_cast<uint8_t>(g - n * 2) : 0;
  b = b > n ? static_cast<uint8_t>(b - n) : 0;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

static uint16_t lighten(uint16_t c, uint8_t n) {
  uint8_t r = static_cast<uint8_t>((c >> 11) & 0x1F);
  uint8_t g = static_cast<uint8_t>((c >> 5) & 0x3F);
  uint8_t b = static_cast<uint8_t>(c & 0x1F);
  r = r + n > 31 ? 31 : static_cast<uint8_t>(r + n);
  g = g + n * 2 > 63 ? 63 : static_cast<uint8_t>(g + n * 2);
  b = b + n > 31 ? 31 : static_cast<uint8_t>(b + n);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

bool uiTimeSynced() {
  return time(nullptr) > 1700000000;
}

void uiFillClock(char *buf, size_t n) {
  time_t now = time(nullptr);
  struct tm t;
  if (now > 1700000000 && localtime_r(&now, &t)) {
    snprintf(buf, n, "%02d:%02d", t.tm_hour, t.tm_min);
    return;
  }
  const unsigned long sec = millis() / 1000UL;
  const int hh = static_cast<int>((8 + sec / 3600UL) % 24);
  const int mm = static_cast<int>((sec / 60UL) % 60);
  snprintf(buf, n, "%02d:%02d", hh, mm);
}

static void nowClock(char *buf, size_t n) {
  uiFillClock(buf, n);
}

static void drawVersion(uint16_t bg) {
  const int16_t tw = textW(BW_VERSION);
  const int16_t x = static_cast<int16_t>(gfx->width() - tw - 8);
  uiText(x, 6, BW_VERSION, 0x07FF, bg, false);
}

static void homeCalBox(int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
  const char *lab = tr("校准", "Cal");
  const int16_t tw = textW(lab);
  const int16_t pw = static_cast<int16_t>(tw + 16);
  if (landscape) {
    *x = 12;
    *y = 74;
    *w = pw;
    *h = 24;
  } else {
    *x = static_cast<int16_t>(gfx->width() - pw - 8);
    *y = 52;
    *w = pw;
    *h = 24;
  }
}

static void drawHomeCalChip() {
  int16_t x = 0, y = 0, w = 0, h = 0;
  homeCalBox(&x, &y, &w, &h);
  gfx->fillRoundRect(x, y, w, h, 12, 0x0328);
  uiText(static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 5), tr("校准", "Cal"), 0x07E8, 0x0328, false);
}

bool uiHitHomeCal(int16_t px, int16_t py) {
  int16_t x = 0, y = 0, w = 0, h = 0;
  homeCalBox(&x, &y, &w, &h);
  return px >= x && px <= static_cast<int16_t>(x + w) && py >= y && py <= static_cast<int16_t>(y + h);
}

bool uiHitTalk(int16_t x, int16_t y) {
#ifdef BW_USE_LVGL
  return bwUiHitTalk(x, y);
#else
  return x >= 16 && x <= 224 && y >= 262 && y <= 318;
#endif
}

static void iconMoon(int16_t cx, int16_t cy, uint16_t fg, uint16_t bg) {
  gfx->fillCircle(cx, cy, 12, fg);
  gfx->fillCircle(cx + 6, cy - 3, 10, bg);
}

static void iconSun(int16_t cx, int16_t cy, uint16_t fg) {
  gfx->fillCircle(cx, cy, 7, fg);
  for (int i = 0; i < 8; i++) {
    const float a = static_cast<float>(i) * 0.785398f;
    const int16_t x0 = static_cast<int16_t>(cx + cosf(a) * 10);
    const int16_t y0 = static_cast<int16_t>(cy + sinf(a) * 10);
    const int16_t x1 = static_cast<int16_t>(cx + cosf(a) * 14);
    const int16_t y1 = static_cast<int16_t>(cy + sinf(a) * 14);
    gfx->drawLine(x0, y0, x1, y1, fg);
    gfx->drawLine(x0 + 1, y0, x1 + 1, y1, fg);
  }
}

static void iconSpeaker(int16_t cx, int16_t cy, uint16_t fg) {
  gfx->fillTriangle(cx - 8, cy, cx - 2, cy - 8, cx - 2, cy + 8, fg);
  gfx->fillRect(cx - 10, cy - 4, 8, 8, fg);
  gfx->drawCircle(cx + 2, cy, 6, fg);
  gfx->drawCircle(cx + 2, cy, 9, fg);
}

static void applyPwm() {
  if (!backlightOn) {
    ledcWrite(BL_LEDC_CH, 0);
    return;
  }
  // Use solid on for mid/high — PWM can look like whole-screen shimmer on some panels.
  switch (brightLevel) {
    case BRIGHT_LOW:
      ledcWrite(BL_LEDC_CH, 140);
      break;
    case BRIGHT_HIGH:
      ledcWrite(BL_LEDC_CH, 255);
      break;
    default:
      ledcWrite(BL_LEDC_CH, 255);
      break;
  }
}

static void printClock() {
  char clock[8];
  nowClock(clock, sizeof(clock));
  if (landscape) {
    gfx->fillRect(8, 4, 124, 40, lastBg);
    uiClock(12, 6, clock, 0xFFFF, lastBg, u8g2_font_logisoso32_tn, 32);
    drawVersion(lastBg);
  } else {
    u8f.setFont(u8g2_font_logisoso42_tn);
    const int16_t cw = static_cast<int16_t>(u8f.getUTF8Width(clock));
    const int16_t x = static_cast<int16_t>((gfx->width() - cw) / 2);
    gfx->fillRect(0, 4, gfx->width(), 48, lastBg);
    uiClock(x, 8, clock, 0xFFFF, lastBg, u8g2_font_logisoso42_tn, 42);
    drawVersion(lastBg);
  }
}

static void drawDndPill(int16_t x, int16_t y) {
  const char *lab = tr("勿扰", "DND");
  const int16_t tw = textW(lab);
  const int16_t pw = static_cast<int16_t>(tw + 16);
  gfx->fillRoundRect(x, y, pw, 22, 11, 0x7810);
  uiText(static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 4), lab, 0xFFFF, 0x7810, false);
}

static void printHud() {
  printClock();
  if (landscape) {
    gfx->fillRect(8, 46, 124, 26, lastBg);
    if (alarmBadge[0]) {
      uiText(12, 48, alarmBadge, 0xFD20, lastBg, false);
    } else if (flagDnd) {
      drawDndPill(12, 46);
    }
  } else {
    gfx->fillRect(0, 52, gfx->width(), 26, lastBg);
    if (alarmBadge[0]) {
      const int16_t tw = textW(alarmBadge);
      uiText(static_cast<int16_t>((gfx->width() - tw) / 2), 54, alarmBadge, 0xFD20, lastBg, false);
    } else if (flagDnd) {
      const int16_t tw = textW(tr("勿扰", "DND"));
      drawDndPill(static_cast<int16_t>((gfx->width() - (tw + 16)) / 2), 52);
    }
  }
  drawHomeCalChip();
}

static void printFooter() {
  const char *line = lastLine[0] ? lastLine : hintLine;
  if (!line || !line[0]) {
    return;
  }
  const int16_t tw = textW(line);
  if (landscape) {
    uiText(12, gfx->height() - 22, line, 0x8C71, lastBg, false);
  } else {
    int16_t x = static_cast<int16_t>((gfx->width() - tw) / 2);
    if (x < 8) {
      x = 8;
    }
    uiText(x, gfx->height() - 26, line, 0x8C71, lastBg, false);
  }
}

static void printFooterFresh() {
  if (landscape) {
    gfx->fillRect(0, gfx->height() - 26, 130, 26, lastBg);
  } else {
    gfx->fillRect(0, gfx->height() - 30, gfx->width(), 30, lastBg);
  }
  printFooter();
}

static void limb(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  for (int i = 0; i <= 8; i++) {
    const int16_t x = static_cast<int16_t>(x0 + (x1 - x0) * i / 8);
    const int16_t y = static_cast<int16_t>(y0 + (y1 - y0) * i / 8);
    gfx->fillCircle(x, y, 6, color);
  }
  gfx->fillCircle(x1, y1, 8, color);
}

static void drawEyes(int16_t cx, int16_t cy, int16_t ox, bool blink) {
  if (blink) {
    gfx->fillRoundRect(cx - ox - 8, cy - 2, 18, 4, 2, 0x0000);
    gfx->fillRoundRect(cx + ox - 10, cy - 2, 18, 4, 2, 0x0000);
    return;
  }
  gfx->fillCircle(cx - ox, cy, 8, 0xFFFF);
  gfx->fillCircle(cx + ox, cy, 8, 0xFFFF);
  gfx->fillCircle(cx - ox + 1, cy + 1, 4, 0x0000);
  gfx->fillCircle(cx + ox + 1, cy + 1, 4, 0x0000);
  gfx->fillCircle(cx - ox - 2, cy - 2, 2, 0xFFFF);
  gfx->fillCircle(cx + ox - 2, cy - 2, 2, 0xFFFF);
}

static void drawPortrait(const Face &face, bool blink, bool talk, int wave, int hop, int shake) {
  const int16_t cx = gfx->width() / 2 + shake;
  const int16_t cy = 158 - hop;
  const uint16_t shade = darken(face.fill, 4);
  const uint16_t hi = lighten(face.fill, 6);

  const uint16_t shadow = (lastBg == 0x0000) ? static_cast<uint16_t>(0x1082) : darken(lastBg, 3);
  gfx->fillEllipse(cx, cy + 102, 38, 9, shadow);

  gfx->fillCircle(cx, cy + 58, 30, shade);
  gfx->fillCircle(cx, cy + 56, 28, face.fill);
  gfx->fillRoundRect(cx - 16, cy + 78, 12, 32, 6, face.fill);
  gfx->fillRoundRect(cx + 4, cy + 78, 12, 32, 6, face.fill);
  limb(cx - 22, cy + 44, cx - 56, cy + 74, face.fill);
  limb(cx + 22, cy + 40, static_cast<int16_t>(cx + 52 + wave), static_cast<int16_t>(cy + 12 - abs(wave)), face.fill);

  gfx->fillCircle(cx - 38, cy - 36, 16, shade);
  gfx->fillCircle(cx + 38, cy - 36, 16, shade);
  gfx->fillCircle(cx - 38, cy - 38, 15, face.fill);
  gfx->fillCircle(cx + 38, cy - 38, 15, face.fill);
  gfx->fillCircle(cx, cy - 6, 56, shade);
  gfx->fillCircle(cx, cy - 8, 52, face.fill);
  gfx->fillCircle(cx - 16, cy - 26, 10, hi);

  gfx->fillCircle(cx - 22, cy + 2, 6, 0xFBAE);
  gfx->fillCircle(cx + 22, cy + 2, 6, 0xFBAE);
  drawEyes(cx, cy - 14, 16, blink);
  gfx->fillCircle(cx, cy - 54, 6, 0xFFE0);
  gfx->fillCircle(cx + 3, cy - 57, 2, 0xFFFF);

  if (talk) {
    const int mouth = 8 + static_cast<int>(micSmooth / 80.0f);
    gfx->fillCircle(cx, cy + 14, mouth > 16 ? 16 : mouth, 0x0000);
  } else if (lastEmotion == EMO_THINK) {
    gfx->fillCircle(cx - 10, cy + 14, 3, 0x0000);
    gfx->fillCircle(cx + 10, cy + 12, 3, 0x0000);
    gfx->fillCircle(cx, cy + 18, 5, 0x0000);
    gfx->fillCircle(cx, cy + 12, 6, face.fill);
    gfx->fillCircle(cx + 48, cy - 48, 4, 0xFFFF);
    gfx->fillCircle(cx + 58, cy - 62, 3, 0xFFFF);
    gfx->fillCircle(cx + 66, cy - 74, 2, 0xFFFF);
  } else if (lastEmotion == EMO_SILENT || lastEmotion == EMO_IDLE) {
    gfx->fillRoundRect(cx - 12, cy + 12, 24, 5, 2, 0x0000);
    gfx->fillRect(cx - 12, cy + 12, 24, 2, face.fill);
  } else {
    gfx->fillRoundRect(cx - 12, cy + 12, 24, 4, 2, 0x0000);
  }
}

static void drawLandscapeFace(const Face &face, bool blink, bool talk) {
  const int16_t cx = 214;
  const int16_t cy = 118;
  const uint16_t shade = darken(face.fill, 4);
  const uint16_t hi = lighten(face.fill, 6);

  gfx->fillCircle(cx - 56, cy - 46, 20, shade);
  gfx->fillCircle(cx + 56, cy - 46, 20, shade);
  gfx->fillCircle(cx - 56, cy - 48, 18, face.fill);
  gfx->fillCircle(cx + 56, cy - 48, 18, face.fill);
  gfx->fillCircle(cx, cy - 2, 78, shade);
  gfx->fillCircle(cx, cy - 4, 72, face.fill);
  gfx->fillCircle(cx - 22, cy - 28, 14, hi);
  gfx->fillCircle(cx - 28, cy + 10, 8, 0xFBAE);
  gfx->fillCircle(cx + 28, cy + 10, 8, 0xFBAE);
  drawEyes(cx, cy - 14, 22, blink);
  gfx->fillCircle(cx, cy - 68, 8, 0xFFE0);
  gfx->fillCircle(cx + 4, cy - 72, 3, 0xFFFF);

  if (talk) {
    const int mouth = 10 + static_cast<int>(micSmooth / 80.0f);
    gfx->fillCircle(cx, cy + 24, mouth > 20 ? 20 : mouth, 0x0000);
  } else if (lastEmotion == EMO_THINK) {
    gfx->fillCircle(cx - 12, cy + 22, 4, 0x0000);
    gfx->fillCircle(cx + 12, cy + 18, 4, 0x0000);
    gfx->fillCircle(cx, cy + 28, 6, 0x0000);
    gfx->fillCircle(cx, cy + 20, 8, face.fill);
    gfx->fillCircle(cx + 70, cy - 40, 5, 0xFFFF);
    gfx->fillCircle(cx + 82, cy - 54, 3, 0xFFFF);
  } else {
    gfx->fillCircle(cx - 14, cy + 30, 5, 0x0000);
    gfx->fillCircle(cx + 14, cy + 30, 5, 0x0000);
    gfx->fillCircle(cx, cy + 34, 8, 0x0000);
    gfx->fillCircle(cx, cy + 26, 10, face.fill);
  }
}

static int16_t micNormFill(int16_t barInnerW) {
  float excess = micSmooth - micFloor * 1.2f;
  if (excess < 0.0f) {
    excess = 0.0f;
  }
  const float span = micFloor * 6.0f + 120.0f;
  float t = excess / span;
  if (t > 1.0f) {
    t = 1.0f;
  }
  t = sqrtf(t);
  return static_cast<int16_t>(t * static_cast<float>(barInnerW) + 0.5f);
}

static uint16_t micBarColor(float t) {
  if (t < 0.35f) {
    return 0x7BEF;
  }
  if (t < 0.75f) {
    return 0x07F9;
  }
  if (t < 0.92f) {
    return 0xFE60;
  }
  return 0xF980;
}

static void drawMicMeter() {
  const int16_t x = 8;
  const int16_t y = 42;
  const int16_t barX = x + 22;
  const int16_t barY = y + 8;
  const int16_t barW = gfx->width() - barX - 10;
  const int16_t barH = 10;
  const int16_t inner = barW - 2;

  // Avoid full-strip erase on every update (visible flicker on SPI LCDs).
  static bool iconDrawn = false;
  if (!iconDrawn || micFillDrawn < 0) {
    gfx->fillRect(0, 40, 28, 26, lastBg);
    const uint16_t micCol = 0xDEFB;
    gfx->fillRoundRect(x + 4, y + 2, 8, 12, 4, micCol);
    gfx->drawRoundRect(x + 1, y + 8, 14, 10, 4, micCol);
    gfx->fillRect(x + 2, y + 8, 12, 5, lastBg);
    gfx->drawFastVLine(x + 8, y + 17, 5, micCol);
    gfx->drawFastHLine(x + 4, y + 22, 8, micCol);
    iconDrawn = true;
  }

  const int16_t fill = micNormFill(inner);
  const float t = inner > 0 ? static_cast<float>(fill) / static_cast<float>(inner) : 0.0f;
  const uint16_t col = micBarColor(t);

  gfx->fillRoundRect(barX, barY, barW, barH, 4, 0x18C3);
  if (fill > 0) {
    const int16_t fw = fill < 6 ? 6 : fill;
    gfx->fillRoundRect(barX + 1, barY + 1, fw > inner ? inner : fw, barH - 2, 3, col);
  }
  micFillDrawn = fill;
  micRmsDrawn = micRms;
}

void uiBegin() {
  ledcSetup(BL_LEDC_CH, 40000, 8);
  ledcAttachPin(PIN_LCD_BLK, BL_LEDC_CH);
  backlightOn = true;
  brightLevel = BRIGHT_MID;
  applyPwm();

  lcdSpi.begin(PIN_LCD_SCK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
  tft.init(240, 320);
  // 40 MHz is flaky on breadboard Dupont wires → tearing/glitches that look like flicker.
  tft.setSPISpeed(20000000);
  tft.setRotation(0);
  gfx->fillScreen(0x0000);
  u8f.begin(*gfx);
  u8f.setFontMode(0);
  u8f.setFontDirection(0);
#ifdef BW_USE_LVGL
  landscape = false;
  tft.setRotation(0);
  lvPortBegin(&tft);
  bwUiStart();
#endif
}

void uiSetBacklight(bool on) {
  backlightOn = on;
  applyPwm();
}

void uiSetBrightnessLevel(uint8_t level) {
  brightLevel = level > BRIGHT_HIGH ? BRIGHT_MID : level;
  applyPwm();
}

void uiSetLandscape(bool on) {
#ifdef BW_USE_LVGL
  (void)on;
  landscape = false;
  tft.setRotation(0);
  touchSetRotation(0);
#else
  landscape = on;
  tft.setRotation(on ? 1 : 0);
  touchSetRotation(on ? 1 : 0);
#endif
}

bool uiLandscape() {
  return landscape;
}

void uiSetFlags(bool wifi, bool lte, bool dnd) {
  flagWifi = wifi;
  flagLte = lte;
  flagDnd = dnd;
}

float uiMicSmooth() {
  return micSmooth;
}

void uiSetRecord(bool on, uint8_t level, const char *status) {
#ifdef BW_USE_LVGL
  bwUiSetRecord(on, level, status);
#else
  (void)on;
  (void)level;
  (void)status;
#endif
}

void uiPatchRecLevel(uint8_t level) {
#ifdef BW_USE_LVGL
  bwUiPatchRecLevel(level);
#else
  (void)level;
#endif
}

void uiSetHint(const char *hint) {
  strncpy(hintLine, hint ? hint : "", sizeof(hintLine) - 1);
  hintLine[sizeof(hintLine) - 1] = 0;
}

void uiSetPair(const char *line) {
#ifdef BW_USE_LVGL
  bwUiSetPair(line);
#else
  (void)line;
#endif
}

void uiSetAlarmBadge(const char *badge) {
  const char *src = badge ? badge : "";
  if (strncmp(alarmBadge, src, sizeof(alarmBadge)) == 0) {
    return;
  }
  strncpy(alarmBadge, src, sizeof(alarmBadge) - 1);
  alarmBadge[sizeof(alarmBadge) - 1] = 0;
}

void uiSetMicLevel(uint16_t rms) {
  if (rms > 20000) {
    return;
  }
  micRms = rms;
  if (static_cast<float>(rms) < micFloor * 1.8f) {
    micFloor = 0.92f * micFloor + 0.08f * static_cast<float>(rms);
  } else if (micFloor < 40.0f) {
    micFloor = 40.0f;
  }
  if (micFloor < 20.0f) {
    micFloor = 20.0f;
  }
  if (micFloor > 2000.0f) {
    micFloor = 2000.0f;
  }
  micSmooth = 0.55f * micSmooth + 0.45f * static_cast<float>(rms);

#ifdef BW_USE_LVGL
  uint8_t level = 0;
  if (micSmooth > 40.0f) {
    const float n = (micSmooth - 40.0f) / 3600.0f;
    level = n >= 1.0f ? 100 : static_cast<uint8_t>(n * 100.0f);
  }
  if (lastEmotion == EMO_LISTEN) {
    bwUiPatchRecLevel(level);
  }
  return;
#else
  // Only redraw when visible bar width changes — RMS noise was flashing the strip.
  const int16_t barInner = gfx->width() - 42;
  const int16_t fill = micNormFill(barInner > 2 ? barInner - 2 : 1);
  if (fill == micFillDrawn) {
    return;
  }
  drawMicMeter();
#endif
}

void uiBlank() {
#ifdef BW_USE_LVGL
  tft.fillScreen(0x0000);
#else
  gfx->fillScreen(0x0000);
#endif
}

int16_t uiWidth() {
  return gfx->width();
}

int16_t uiHeight() {
  return gfx->height();
}

void uiResumeHome() {
#ifdef BW_USE_LVGL
  bwUiShowHome(lastLine[0] ? lastLine : tr("右滑菜单 · 点按钮", "swipe right · tap"), 0);
#endif
}

void uiShow(Emotion emotion, const char *subtitle) {
  const Face face = faceOf(emotion);
  lastEmotion = emotion;
  lastBg = face.bg;
  strncpy(lastLine, subtitle ? subtitle : "", sizeof(lastLine) - 1);
  lastLine[sizeof(lastLine) - 1] = 0;
#ifdef BW_USE_LVGL
  uint32_t hex = 0x5E9D9A;
  switch (emotion) {
    case EMO_LISTEN:
      hex = 0xE53935;
      break;
    case EMO_THINK:
      hex = 0x9C27B0;
      break;
    case EMO_SPEAK:
      hex = 0xFF9800;
      break;
    case EMO_SILENT:
    case EMO_QUIET:
      hex = 0x555555;
      break;
    case EMO_ALARM:
      hex = 0xF44336;
      break;
    case EMO_OFFLINE:
      hex = 0x607D8B;
      break;
    default:
      hex = 0x5E9D9A;
      break;
  }
  if (emotion == EMO_LISTEN) {
    bwUiSetRecord(true, 0, subtitle);
  } else {
    bwUiSetRecord(false, 0, nullptr);
  }
  bwUiShowHome(subtitle, hex);
  {
    const char *name = "idle";
    switch (emotion) {
      case EMO_LISTEN:
        name = "listen";
        break;
      case EMO_THINK:
        name = "think";
        break;
      case EMO_SPEAK:
        name = "speak";
        break;
      case EMO_SILENT:
        name = "silent";
        break;
      case EMO_QUIET:
        name = "quiet";
        break;
      case EMO_ALARM:
        name = "alarm";
        break;
      case EMO_OFFLINE:
        name = "offline";
        break;
      default:
        break;
    }
    if (netCloudUp()) {
      netPublishFace(name, lastLine);
    }
  }
#else
  micFillDrawn = -1;
  gfx->fillScreen(face.bg);
  printHud();
#if VOICE_FEATURES
  drawMicMeter();
#endif
  const bool talk = isTalking(emotion);
  if (landscape) {
    drawLandscapeFace(face, false, talk);
  } else {
    drawPortrait(face, false, talk, 10, 4, 0);
  }
  printFooterFresh();
#endif
}

void uiFlush() {
#ifdef BW_USE_LVGL
  bwUiFlush();
#endif
}

void uiTick() {
#ifdef BW_USE_LVGL
  bwUiTick();
  return;
#endif
  // Home must stay visually static. Only patch the clock when the minute changes,
  // and the alarm badge when its text changes — never wipe footer/character.
  static int lastMm = -1;
  static char lastBadgeDrawn[16] = "";
  const unsigned long sec = millis() / 1000UL;
  const int mm = static_cast<int>((sec / 60UL) % 60);

  if (mm != lastMm) {
    lastMm = mm;
    printClock();
  }

  if (strncmp(lastBadgeDrawn, alarmBadge, sizeof(lastBadgeDrawn)) != 0) {
    strncpy(lastBadgeDrawn, alarmBadge, sizeof(lastBadgeDrawn) - 1);
    lastBadgeDrawn[sizeof(lastBadgeDrawn) - 1] = 0;
    printHud();
  }

#if VOICE_FEATURES
  static unsigned long lastMicUi = 0;
  const unsigned long now = millis();
  if (now - lastMicUi >= 120) {
    lastMicUi = now;
    const int16_t barInner = gfx->width() - 42;
    const int16_t fill = micNormFill(barInner > 2 ? barInner - 2 : 1);
    if (fill != micFillDrawn) {
      drawMicMeter();
    }
  }
#endif
}

static void sheetChrome(const char *title) {
  const int16_t w = gfx->width();
  gfx->fillRoundRect(w / 2 - 18, 8, 36, 4, 2, 0x4A49);
  uiText(16, 20, title, 0xFFFF, 0x0000, true);
  drawVersion(0x0000);
}

static void pageTitle(const char *title) {
  sheetChrome(title);
}

static int16_t landColW() {
  return static_cast<int16_t>((gfx->width() - 16) / 3);
}

static int16_t landColX(int i) {
  return static_cast<int16_t>(8 + i * landColW());
}

static void drawSettingsList(uint8_t scroll, uint8_t selected, bool offline, const char *touchMode) {
  (void)scroll;
  (void)offline;
  (void)touchMode;
  const uint16_t card = 0x2104;
  const uint16_t cardSel = 0x39C7;
  const char *labels[] = {tr("触摸校准", "Touch cal"), tr("屏幕", "Screen"), tr("语言", "Language")};
  const char *values[] = {tr("开始", "Start"), prefs().landscape ? tr("横屏", "Land") : tr("竖屏", "Port"),
                          prefsLangName()};
  if (landscape) {
    gfx->fillRect(0, 36, gfx->width(), gfx->height() - 36, 0x0000);
    const int16_t cw = landColW();
    const int16_t ch = gfx->height() - 52;
    for (int i = 0; i < UI_SET_N; i++) {
      const int16_t x = landColX(i);
      uint16_t fill = (i == selected) ? cardSel : card;
      if (i == 0) {
        fill = (i == selected) ? static_cast<uint16_t>(0x0540) : static_cast<uint16_t>(0x0328);
      }
      gfx->fillRoundRect(x, UI_LAND_TILE_Y, cw - 8, ch, 20, fill);
      uiText(x + 12, 70, labels[i], 0xFFFF, fill, true);
      if (values[i][0]) {
        uiText(x + 12, 108, values[i], i == 0 ? static_cast<uint16_t>(0x07E8) : static_cast<uint16_t>(0x8C71), fill,
               true);
      }
    }
    return;
  }
  gfx->fillRect(0, UI_SET_TOP - 4, gfx->width(), UI_SET_N * UI_SET_ROW + 8, 0x0000);
  for (int i = 0; i < UI_SET_N; i++) {
    const int16_t y = UI_SET_TOP + i * UI_SET_ROW;
    uint16_t fill = (i == selected) ? cardSel : card;
    if (i == 0) {
      fill = (i == selected) ? static_cast<uint16_t>(0x0540) : static_cast<uint16_t>(0x0328);
    }
    gfx->fillRoundRect(10, y, gfx->width() - 20, UI_SET_ROW - 12, 20, fill);
    uiText(24, y + 24, labels[i], 0xFFFF, fill, true);
    if (values[i][0]) {
      const int16_t vw = textW(values[i]);
      uiText(static_cast<int16_t>(gfx->width() - 24 - vw), y + 24, values[i],
             i == 0 ? static_cast<uint16_t>(0x07E8) : static_cast<uint16_t>(0x8C71), fill, true);
    }
  }
}

void uiDrawMenu() {
#ifdef BW_USE_LVGL
  bwUiShowMenu();
#else
  (void)0;
#endif
}

void uiDrawLteTest() {
#ifdef BW_USE_LVGL
  bwUiShowLte();
#endif
}

void uiPatchLteTest(const char *status, const char *id, const char *sim, const char *csq, const char *hint,
                    uint32_t statusColor) {
#ifdef BW_USE_LVGL
  bwUiPatchLte(status, id, sim, csq, hint, statusColor);
#else
  (void)status;
  (void)id;
  (void)sim;
  (void)csq;
  (void)hint;
  (void)statusColor;
#endif
}

void uiDrawSettings(uint8_t scroll, uint8_t selected, bool offline, const char *touchMode) {
#ifdef BW_USE_LVGL
  (void)scroll;
  (void)selected;
  (void)offline;
  (void)touchMode;
  bwUiShowSettings();
  return;
#endif
  gfx->fillScreen(0x0000);
  pageTitle(tr("设置", "Settings"));
  drawSettingsList(scroll, selected, offline, touchMode);
}

void uiPatchSettings(uint8_t scroll, uint8_t selected, bool offline, const char *touchMode) {
#ifdef BW_USE_LVGL
  (void)scroll;
  (void)selected;
  (void)offline;
  bwUiPatchSettings(prefs().landscape ? tr("横屏", "Land") : tr("竖屏", "Port"), prefsLangName(),
                    touchMode);
  return;
#endif
  drawSettingsList(scroll, selected, offline, touchMode);
}

static void paintControlRows(uint8_t volume, uint8_t brightness, bool dnd) {
  (void)volume;
  (void)brightness;
  char b[12];
  char v[12];
  snprintf(b, sizeof(b), "%s", prefsBrightnessName());
  snprintf(v, sizeof(v), "%s", prefsVolumeName());
  const char *labels[] = {tr("勿扰", "DND"), tr("亮度", "Bright"), tr("声音", "Sound")};
  const char *vals[] = {dnd ? tr("开", "On") : tr("关", "Off"), b, v};
  const uint16_t fills[] = {static_cast<uint16_t>(dnd ? 0x6113 : 0x2104), 0x3A00, 0x0320};
  const uint16_t vcols[] = {static_cast<uint16_t>(dnd ? 0xFFFF : 0x8C71), 0xFFE0, 0x07E8};
  if (landscape) {
    gfx->fillRect(0, 36, gfx->width(), gfx->height() - 36, 0x0000);
    const int16_t cw = landColW();
    const int16_t ch = gfx->height() - 52;
    for (int i = 0; i < UI_CTRL_N; i++) {
      const int16_t x = landColX(i);
      gfx->fillRoundRect(x, UI_LAND_TILE_Y, cw - 8, ch, 20, fills[i]);
      const int16_t icx = static_cast<int16_t>(x + (cw - 8) / 2);
      if (i == 0) {
        iconMoon(icx, 88, 0xFFFF, fills[i]);
      } else if (i == 1) {
        iconSun(icx, 88, 0xFFE0);
      } else {
        iconSpeaker(icx, 88, 0x07E8);
      }
      uiText(x + 12, 118, labels[i], 0xFFFF, fills[i], true);
      uiText(x + 12, 142, vals[i], vcols[i], fills[i], true);
    }
    return;
  }
  gfx->fillRect(0, UI_CTRL_TOP - 4, gfx->width(), UI_CTRL_N * UI_CTRL_ROW + 8, 0x0000);
  for (int i = 0; i < UI_CTRL_N; i++) {
    const int16_t y = UI_CTRL_TOP + i * UI_CTRL_ROW;
    gfx->fillRoundRect(10, y, gfx->width() - 20, UI_CTRL_ROW - 12, 20, fills[i]);
    const int16_t icy = static_cast<int16_t>(y + (UI_CTRL_ROW - 12) / 2);
    if (i == 0) {
      iconMoon(40, icy, 0xFFFF, fills[i]);
    } else if (i == 1) {
      iconSun(40, icy, 0xFFE0);
    } else {
      iconSpeaker(40, icy, 0x07E8);
    }
    uiText(68, y + 14, labels[i], 0xFFFF, fills[i], true);
    uiText(68, y + 36, vals[i], vcols[i], fills[i], true);
  }
}

void uiDrawControl(uint8_t volume, uint8_t brightness, bool dnd) {
#ifdef BW_USE_LVGL
  (void)volume;
  (void)brightness;
  (void)dnd;
  bwUiShowControl();
  return;
#endif
  gfx->fillScreen(0x0000);
  pageTitle(tr("控制", "Control"));
  paintControlRows(volume, brightness, dnd);
}

int8_t uiHitBand(int16_t y, int16_t top, int16_t rowH, uint8_t count) {
  if (rowH <= 0 || count == 0 || y < top) {
    return -1;
  }
  const int row = (y - top) / rowH;
  if (row < 0 || row >= static_cast<int>(count)) {
    return -1;
  }
  return static_cast<int8_t>(row);
}

int8_t uiHitControl(int16_t x, int16_t y) {
  if (landscape) {
    if (y < UI_LAND_TILE_Y || y > gfx->height() - 8) {
      return -1;
    }
    return uiHitBand(x, 8, landColW(), UI_CTRL_N);
  }
  return uiHitBand(y, UI_CTRL_TOP, UI_CTRL_ROW, UI_CTRL_N);
}

int8_t uiHitSettings(int16_t x, int16_t y) {
  if (landscape) {
    if (y < UI_LAND_TILE_Y || y > gfx->height() - 8) {
      return -1;
    }
    return uiHitBand(x, 8, landColW(), UI_SET_N);
  }
  return uiHitBand(y, UI_SET_TOP, UI_SET_ROW, UI_SET_N);
}

void uiCalGeom(UiCalGeom *out) {
  if (!out) {
    return;
  }
  if (landscape) {
    out->gx = 12;
    out->gy = 56;
    out->gw = 84;
    out->gh = 84;
    out->bx = static_cast<int16_t>(gfx->width() - 96);
    out->by = 108;
    out->bw = 84;
    out->bh = 84;
    out->doneY = static_cast<int16_t>(gfx->height() - 42);
  } else {
    out->gx = 12;
    out->gy = 78;
    out->gw = 110;
    out->gh = 110;
    out->bx = 118;
    out->by = 178;
    out->bw = 110;
    out->bh = 110;
    out->doneY = 248;
  }
}

void uiCalTargets(int16_t *t0x, int16_t *t0y, int16_t *t1x, int16_t *t1y) {
  UiCalGeom g{};
  uiCalGeom(&g);
  if (t0x) {
    *t0x = static_cast<int16_t>(g.gx + g.gw / 2);
  }
  if (t0y) {
    *t0y = static_cast<int16_t>(g.gy + g.gh / 2);
  }
  if (t1x) {
    *t1x = static_cast<int16_t>(g.bx + g.bw / 2);
  }
  if (t1y) {
    *t1y = static_cast<int16_t>(g.by + g.bh / 2);
  }
}

bool uiCalInGreen(int16_t x, int16_t y) {
  UiCalGeom g{};
  uiCalGeom(&g);
  return x >= g.gx && x <= static_cast<int16_t>(g.gx + g.gw) && y >= g.gy &&
         y <= static_cast<int16_t>(g.gy + g.gh);
}

bool uiCalInBlue(int16_t x, int16_t y) {
  UiCalGeom g{};
  uiCalGeom(&g);
  return x >= g.bx && x <= static_cast<int16_t>(g.bx + g.bw) && y >= g.by &&
         y <= static_cast<int16_t>(g.by + g.bh);
}

void uiPatchControl(uint8_t volume, uint8_t brightness, bool dnd) {
#ifdef BW_USE_LVGL
  bwUiPatchControl(volume, brightness, dnd);
  return;
#endif
  paintControlRows(volume, brightness, dnd);
}

void uiPatchSubtitle(const char *subtitle) {
  strncpy(lastLine, subtitle ? subtitle : "", sizeof(lastLine) - 1);
  lastLine[sizeof(lastLine) - 1] = 0;
#ifdef BW_USE_LVGL
  bwUiSetSubtitle(subtitle);
  return;
#endif
  printFooterFresh();
}

static void drawTarget(int16_t cx, int16_t cy, int16_t r, uint16_t col) {
  gfx->fillCircle(cx, cy, r, col);
  gfx->fillCircle(cx, cy, static_cast<int16_t>(r - 12), 0x0000);
  gfx->fillCircle(cx, cy, 7, col);
}

static void drawCalDots(uint8_t step) {
  const int16_t x0 = static_cast<int16_t>(gfx->width() - 52);
  for (uint8_t i = 0; i < 3; i++) {
    const uint16_t c = (i == step) ? static_cast<uint16_t>(0xFFFF) : static_cast<uint16_t>(0x4208);
    gfx->fillCircle(static_cast<int16_t>(x0 + i * 16), 22, 3, c);
  }
}

void uiDrawTouchCal(uint8_t step, int16_t x, int16_t y, bool down, const char *mapName) {
#ifdef BW_USE_LVGL
  bwUiShowCal();
  bwUiPatchCal(step, x, y, down, mapName);
  return;
#endif
  (void)mapName;
  static uint8_t drawnStep = 0xFF;
  static int16_t lastX = -999;
  static int16_t lastY = -999;
  const uint16_t bg = 0x0000;
  UiCalGeom box{};
  uiCalGeom(&box);
  const int16_t t0x = static_cast<int16_t>(box.gx + box.gw / 2);
  const int16_t t0y = static_cast<int16_t>(box.gy + box.gh / 2);
  const int16_t t1x = static_cast<int16_t>(box.bx + box.bw / 2);
  const int16_t t1y = static_cast<int16_t>(box.by + box.bh / 2);
  const int16_t rad = static_cast<int16_t>((box.gw < box.gh ? box.gw : box.gh) / 2 - 4);

  if (drawnStep != step || x < 0) {
    gfx->fillScreen(bg);
    gfx->fillRoundRect(gfx->width() / 2 - 18, 8, 36, 4, 2, 0x4A49);
    uiText(16, 18, tr("触摸校准", "Touch cal"), 0xFFFF, bg, true);
    drawVersion(bg);
    drawCalDots(step);
    if (step == 0) {
      uiText(16, 40, tr("点左上角准星", "Tap top-left"), 0x8C71, bg, false);
      drawTarget(t0x, t0y, rad, 0x07E0);
    } else if (step == 1) {
      uiText(16, 40, tr("点右下角准星", "Tap bottom-right"), 0x8C71, bg, false);
      drawTarget(t1x, t1y, rad, 0x05FF);
    } else {
      uiText(16, 40, tr("按住绿点确认", "Hold green"), 0x07E0, bg, false);
      drawTarget(t0x, t0y, rad, 0x07E0);
      gfx->fillRoundRect(10, box.doneY, gfx->width() - 20, 36, 18, 0x07E0);
      const char *done = tr("完成", "Done");
      uiText(static_cast<int16_t>((gfx->width() - textW(done)) / 2), box.doneY + 10, done, 0x0000, 0x07E0, true);
    }
    drawnStep = step;
    lastX = -999;
    lastY = -999;
    if (x < 0) {
      return;
    }
  }

  if (step < 2) {
    return;
  }

  if (lastX >= 0) {
    const int16_t ex = lastX > 12 ? lastX - 12 : 0;
    const int16_t ey = lastY > 12 ? lastY - 12 : 0;
    gfx->fillRect(ex, ey, 24, 24, bg);
  }

  const int16_t yMax = static_cast<int16_t>(box.doneY - 8);
  if (down && y > 36 && y < yMax) {
    gfx->fillCircle(x, y, 7, 0xFFE0);
    gfx->drawCircle(x, y, 11, 0xF800);
    lastX = x;
    lastY = y;
  } else if (!down) {
    lastX = -999;
    lastY = -999;
  }
}

static bool ensureShotCanvas() {
  const int w = tft.width();
  const int h = tft.height();
  if (shotCanvas && shotCanvas->getBuffer() && shotCanvas->width() == w && shotCanvas->height() == h) {
    return true;
  }
  delete shotCanvas;
  shotCanvas = new GFXcanvas16(w, h);
  if (!shotCanvas || !shotCanvas->getBuffer()) {
    Serial.println("shot: canvas alloc failed");
    shotCanvas = nullptr;
    return false;
  }
  Serial.printf("shot: canvas ok %u bytes\n", static_cast<unsigned>(w) * static_cast<unsigned>(h) * 2u);
  return true;
}

void uiScreenshotToSerial() {
  if (!ensureShotCanvas()) {
    return;
  }

  Adafruit_GFX *prev = gfx;
  gfx = shotCanvas;
  u8f.begin(*shotCanvas);

  // Redraw current UI into RAM canvas (does not touch the physical LCD).
  if (appScreen() == APP_HOME) {
    uiShow(lastEmotion, lastLine[0] ? lastLine : tr("右滑菜单 · 点按钮", "swipe right · tap"));
  } else {
    appRedraw();
  }

  const int w = shotCanvas->width();
  const int h = shotCanvas->height();
  uint16_t *buf = shotCanvas->getBuffer();
  const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 2u;

  // Binary frame protocol — keep this free of other Serial prints mid-payload.
  Serial.print(">>>BWSHOT\n");
  Serial.printf("%d %d\n", w, h);
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(buf);
  for (size_t off = 0; off < bytes;) {
    size_t n = bytes - off;
    if (n > 512) {
      n = 512;
    }
    Serial.write(raw + off, n);
    off += n;
    delay(0);
  }
  Serial.print("\n<<<BWSHOT\n");

  gfx = prev;
  u8f.begin(tft);

  // Free canvas so we do not sit on 150KB forever; restore live TFT.
  delete shotCanvas;
  shotCanvas = nullptr;
  if (appScreen() == APP_HOME) {
    uiShow(lastEmotion, lastLine[0] ? lastLine : tr("右滑菜单 · 点按钮", "swipe right · tap"));
  } else {
    appRedraw();
  }
}

