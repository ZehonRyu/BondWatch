#include "sim.h"
#include "pins.h"
#include "net.h"

#include <Arduino.h>
#include <WiFi.h>

static bool wifiOn = true;
static bool lteOn = false;
static bool landscape = false;
static int lastSee = HIGH;
static int lastRot = HIGH;

void simBegin() {
  pinMode(PIN_SEE, INPUT_PULLUP);
  pinMode(PIN_ROT, INPUT_PULLUP);
  pinMode(PIN_LTE, OUTPUT);
  digitalWrite(PIN_LTE, lteOn ? HIGH : LOW);
}

static bool falling(int pin, int *last) {
  const int level = digitalRead(pin);
  const bool edge = *last == HIGH && level == LOW;
  *last = level;
  return edge;
}

bool simSeePressed() { return falling(PIN_SEE, &lastSee); }
bool simRotPressed() { return falling(PIN_ROT, &lastRot); }
bool simWifiOn() { return wifiOn && netReady(); }
bool simLteOn() { return lteOn; }
bool simHasNet() { return netReady() || (wifiOn && WiFi.status() == WL_CONNECTED); }
bool simLandscape() { return landscape; }

void simSetWifi(bool on) {
  wifiOn = on;
  Serial.println(on ? "Wi-Fi flag on" : "Wi-Fi flag off");
}

void simSetLte(bool on) {
  lteOn = on;
  digitalWrite(PIN_LTE, on ? HIGH : LOW);
  Serial.println(on ? "4G flag on" : "4G flag off");
}

void simToggleLandscape() {
  simSetLandscape(!landscape);
}

void simSetLandscape(bool on) {
  landscape = on;
  Serial.println(on ? "Landscape" : "Portrait");
}

void simPrintHelp() {
  Serial.println();
  Serial.println("BondWatch  PWR+VOL keys, rest on touch");
  Serial.println("PWR short=screen  double=DND  long=on/off");
  Serial.println("VOL short=volume  long=settings");
  Serial.println("Touch: tap=ack  swipe up=control  left=settings");
  Serial.println("Serial: t talk  c control  g settings  v volume  r rotate");
  Serial.println("        i home  w wake  ! screenshot  h help");
  Serial.println();
}

bool simPollSerial(char *outCmd) {
  if (!Serial.available()) {
    return false;
  }
  *outCmd = static_cast<char>(Serial.read());
  return true;
}
