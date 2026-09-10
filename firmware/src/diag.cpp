// Hardware self-test: onboard RGB + LCD backlight/colors + PWR/VOL.
// Build/upload: pio run -e diag -t upload --upload-port COMx
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "pins.h"

// Goouuu ESP32-S3-N16R8 onboard WS2812
#define PIN_RGB 48

static SPIClass lcdSpi(FSPI);
static Adafruit_ST7789 tft(&lcdSpi, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST);

static void reportButtons() {
  const int pwr = digitalRead(PIN_PWR);
  const int vol = digitalRead(PIN_VOL);
  Serial.printf("PWR(G%d)=%s  VOL(G%d)=%s\n", PIN_PWR, pwr == LOW ? "PRESSED/LOW" : "open/HIGH",
                PIN_VOL, vol == LOW ? "PRESSED/LOW" : "open/HIGH");
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
  Serial.println("=== BondWatch DIAG ===");
  Serial.println("Board RGB LED should cycle R/G/B (GPIO48).");
  Serial.println("Screen should go RED -> GREEN -> BLUE if VCC/GND/BL wired.");
  Serial.printf("LCD: SCK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d\n", PIN_LCD_SCK, PIN_LCD_MOSI,
                PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_BLK);

  pinMode(PIN_PWR, INPUT_PULLUP);
  pinMode(PIN_VOL, INPUT_PULLUP);
  pinMode(PIN_LCD_BLK, OUTPUT);
  digitalWrite(PIN_LCD_BLK, HIGH);
  pinMode(PIN_RGB, OUTPUT);

  neopixelWrite(PIN_RGB, 64, 0, 0);
  Serial.println("RGB RED");
  delay(500);
  neopixelWrite(PIN_RGB, 0, 64, 0);
  Serial.println("RGB GREEN");
  delay(500);
  neopixelWrite(PIN_RGB, 0, 0, 64);
  Serial.println("RGB BLUE");
  delay(500);

  lcdSpi.begin(PIN_LCD_SCK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
  // Waveshare 2.0" = 240x320; 1.69" was 240x280
  tft.init(240, 320);
  tft.setSPISpeed(20000000);
  tft.setRotation(0);
  digitalWrite(PIN_LCD_BLK, HIGH);

  tft.fillScreen(ST77XX_RED);
  Serial.println("fill RED");
  delay(800);
  tft.fillScreen(ST77XX_GREEN);
  Serial.println("fill GREEN");
  delay(800);
  tft.fillScreen(ST77XX_BLUE);
  Serial.println("fill BLUE");
  delay(800);

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.print("BondWatch");
  tft.setCursor(20, 130);
  tft.print("DIAG OK");
  Serial.println("drew BondWatch DIAG OK");
  reportButtons();
  Serial.println("If SCREEN black: VCC->3V3, GND, BL->G13 (or temp BL->3V3)");
  Serial.println("=== DIAG ready ===");
}

void loop() {
  static unsigned long last = 0;
  static uint8_t phase = 0;
  digitalWrite(PIN_LCD_BLK, HIGH);
  const unsigned long now = millis();
  if (now - last >= 800) {
    last = now;
    phase = (phase + 1) % 3;
    if (phase == 0) {
      neopixelWrite(PIN_RGB, 40, 0, 0);
    } else if (phase == 1) {
      neopixelWrite(PIN_RGB, 0, 40, 0);
    } else {
      neopixelWrite(PIN_RGB, 0, 0, 40);
    }
    reportButtons();
  }
}
