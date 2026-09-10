#pragma once

// Copy this file to secrets.h (that file is gitignored).
// 1 = skip WiFi, USB serial only. 0 = try home WiFi + cloud.
#define OFFLINE_USB 1

// 0 = disable mic / VAD / listen-think-speak. UI-only period.
#define VOICE_FEATURES 0

#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#define API_BASE "http://192.168.1.10:11111"
#define API_USER "demo"
#define API_PASS "demo123"
#define DEVICE_ID "watch-esp32-1"
