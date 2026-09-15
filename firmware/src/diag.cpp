// Hardware wiring probe: LCD, I2C touch, INT, keys, optional mic.
// Build/upload: python -m platformio run -e diag -t upload --upload-port COMx
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <driver/i2s.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include "pins.h"

#define PIN_RGB 48
#define CST816_ADDR 0x15
#define REG_GESTURE 0x01
#define REG_CHIP_ID 0xA7

static SPIClass lcdSpi(FSPI);
static Adafruit_ST7789 tft(&lcdSpi, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);

static bool lcdOk = false;
static bool i2cOk = false;
static bool i2cSwappedHint = false;
static uint8_t chipId = 0;
static uint8_t foundAddrs[8];
static uint8_t foundN = 0;
static bool micReady = false;

static void serialLine(const char *fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.println(buf);
}

static void row(int16_t y, uint16_t color, const char *fmt, ...) {
  char buf[36];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  tft.fillRect(0, y, 240, 18, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(color, ST77XX_BLACK);
  tft.setCursor(4, y);
  tft.print(buf);
}

static bool i2cPing(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t *val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  *val = static_cast<uint8_t>(Wire.read());
  return true;
}

static uint8_t scanBus(uint8_t *out, uint8_t maxn) {
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127 && n < maxn; a++) {
    if (i2cPing(a)) {
      out[n++] = a;
    }
  }
  return n;
}

static void startBus(int sda, int scl) {
  Wire.end();
  delay(5);
  Wire.begin(sda, scl);
  Wire.setClock(100000);
  delay(20);
}

static void resetTouch() {
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(20);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(80);
}

static void probeTouch() {
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  resetTouch();
  startBus(PIN_TP_SDA, PIN_TP_SCL);
  foundN = scanBus(foundAddrs, 8);
  i2cOk = i2cPing(CST816_ADDR);
  chipId = 0;
  if (i2cOk) {
    i2cRead8(CST816_ADDR, REG_CHIP_ID, &chipId);
  }
  i2cSwappedHint = false;
  if (!i2cOk && foundN == 0) {
    startBus(PIN_TP_SCL, PIN_TP_SDA); // SDA/SCL swapped guess
    const uint8_t n2 = scanBus(foundAddrs, 8);
    if (n2 > 0 || i2cPing(CST816_ADDR)) {
      i2cSwappedHint = true;
      foundN = n2;
      i2cOk = i2cPing(CST816_ADDR);
    }
    startBus(PIN_TP_SDA, PIN_TP_SCL);
    foundN = scanBus(foundAddrs, 8);
    i2cOk = i2cPing(CST816_ADDR);
    if (i2cOk) {
      i2cRead8(CST816_ADDR, REG_CHIP_ID, &chipId);
    }
  }
}

static void probeMic() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_MIC_SCK;
  pins.ws_io_num = PIN_MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_MIC_SD;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    return;
  }
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    i2s_driver_uninstall(I2S_NUM_0);
    return;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  micReady = true;
}

static uint16_t micRms() {
  if (!micReady) {
    return 0;
  }
  int32_t buf[64];
  size_t bytes = 0;
  if (i2s_read(I2S_NUM_0, buf, sizeof(buf), &bytes, 8) != ESP_OK || bytes < 4) {
    return 0;
  }
  const size_t n = bytes / sizeof(int32_t);
  double acc = 0;
  for (size_t i = 0; i < n; i++) {
    const int32_t s = buf[i] >> 16;
    acc += static_cast<double>(s) * static_cast<double>(s);
  }
  return static_cast<uint16_t>(sqrt(acc / static_cast<double>(n)));
}

static bool readTouch(int16_t *x, int16_t *y, uint8_t *fingers) {
  uint8_t buf[7];
  Wire.beginTransmission(CST816_ADDR);
  Wire.write(REG_GESTURE);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(CST816_ADDR), 7) != 7) {
    return false;
  }
  for (int i = 0; i < 7; i++) {
    buf[i] = static_cast<uint8_t>(Wire.read());
  }
  *fingers = buf[1] & 0x0F;
  *x = static_cast<int16_t>(((buf[2] & 0x0F) << 8) | buf[3]);
  *y = static_cast<int16_t>(((buf[4] & 0x0F) << 8) | buf[5]);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  serialLine("");
  serialLine("=== BondWatch WIRE TEST ===");
  serialLine("Not an OS — this is a probe firmware.");
  serialLine("LCD SCK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d", PIN_LCD_SCK, PIN_LCD_MOSI, PIN_LCD_CS,
             PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_BLK);
  serialLine("TP  SDA=%d SCL=%d INT=%d RST=%d  expect CST816 @ 0x15", PIN_TP_SDA, PIN_TP_SCL,
             PIN_TP_INT, PIN_TP_RST);

  pinMode(PIN_PWR, INPUT_PULLUP);
  pinMode(PIN_VOL, INPUT_PULLUP);
  pinMode(PIN_LCD_BLK, OUTPUT);
  digitalWrite(PIN_LCD_BLK, HIGH);
  pinMode(PIN_RGB, OUTPUT);

  lcdSpi.begin(PIN_LCD_SCK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
  tft.init(240, 320);
  tft.setSPISpeed(20000000);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_RED);
  delay(250);
  tft.fillScreen(ST77XX_GREEN);
  delay(250);
  tft.fillScreen(ST77XX_BLUE);
  delay(250);
  tft.fillScreen(ST77XX_BLACK);
  lcdOk = true;
  serialLine("LCD: PASS (you can see colors)");

  probeTouch();
  if (foundN == 0) {
    serialLine("I2C: FAIL  no device on G%d/G%d", PIN_TP_SDA, PIN_TP_SCL);
    if (i2cSwappedHint) {
      serialLine("I2C: swapped SDA/SCL found a chip — swap TP_SDA and TP_SCL wires");
    } else {
      serialLine("Check: TP_SDA->G14  TP_SCL->G21  TP_RST->G39  touch VCC->3V3  GND");
      serialLine("2.0 panel: pin is TP_INT (same as 1.69 TP_IRQ) -> G38");
    }
  } else {
    char list[64] = {0};
    size_t off = 0;
    for (uint8_t i = 0; i < foundN && off + 8 < sizeof(list); i++) {
      off += static_cast<size_t>(snprintf(list + off, sizeof(list) - off, "0x%02X ", foundAddrs[i]));
    }
    serialLine("I2C scan: %s", list);
    serialLine("CST816 0x15: %s  id=0x%02X", i2cOk ? "PASS" : "MISS", chipId);
  }

  probeMic();
  serialLine("MIC I2S: %s  (WS=%d SCK=%d SD=%d)  0 rms = silent or unwired",
             micReady ? "driver up" : "fail", PIN_MIC_WS, PIN_MIC_SCK, PIN_MIC_SD);
  serialLine("Keys: tap PWR(G%d) and VOL(G%d). Touch the glass.", PIN_PWR, PIN_VOL);
  serialLine("=== live ===");
}

void loop() {
  static unsigned long lastDraw = 0;
  const unsigned long now = millis();
  if (now - lastDraw < 120) {
    return;
  }
  lastDraw = now;

  const int irq = digitalRead(PIN_TP_INT);
  const bool pwr = digitalRead(PIN_PWR) == LOW;
  const bool vol = digitalRead(PIN_VOL) == LOW;
  int16_t x = -1, y = -1;
  uint8_t f = 0;
  bool sample = false;
  if (i2cOk) {
    sample = readTouch(&x, &y, &f);
  }
  const uint16_t rms = micRms();
  const bool finger = sample && f > 0 && f < 3;

  row(6, ST77XX_CYAN, "WIRE TEST");
  row(28, ST77XX_WHITE, "LCD  PASS");
  if (i2cOk) {
    row(50, ST77XX_GREEN, "I2C  0x15 id=%02X", chipId);
  } else if (i2cSwappedHint) {
    row(50, ST77XX_YELLOW, "I2C  SWAP SDA/SCL");
  } else if (foundN) {
    row(50, ST77XX_YELLOW, "I2C  other chip");
  } else {
    row(50, ST77XX_RED, "I2C  FAIL G14/G21");
  }
  row(72, irq == LOW ? ST77XX_GREEN : ST77XX_WHITE, "INT  %s G38", irq == LOW ? "LOW touch?" : "HIGH idle");
  if (finger) {
    row(94, ST77XX_GREEN, "XY   %d,%d f=%u", x, y, f);
  } else {
    row(94, ST77XX_WHITE, "XY   --  tap glass");
  }
  row(116, pwr ? ST77XX_GREEN : ST77XX_WHITE, "PWR  %s G2", pwr ? "DOWN" : "open");
  row(138, vol ? ST77XX_GREEN : ST77XX_WHITE, "VOL  %s G1", vol ? "DOWN" : "open");
  row(160, rms > 40 ? ST77XX_GREEN : ST77XX_WHITE, "MIC  rms=%u", rms);
  row(190, ST77XX_CYAN, i2cOk ? "tap: XY must move" : "fix TP_SDA/SCL/RST");
  row(212, ST77XX_WHITE, "2.0 TP_INT not IRQ");

  serialLine("irq=%s pwr=%d vol=%d i2c=%d xy=%d,%d f=%u rms=%u", irq == LOW ? "LOW" : "HIGH", pwr ? 1 : 0,
             vol ? 1 : 0, i2cOk ? 1 : 0, x, y, f, rms);

  neopixelWrite(PIN_RGB, finger ? 0 : 0, finger ? 40 : 0, lcdOk ? 20 : 0);
}
