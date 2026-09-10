#include "net.h"
#include "secrets.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>

static char token[80] = {0};
static char sessionId[24] = {0};
static bool wifiOk = false;

static Emotion emotionFrom(const char *name) {
  if (!name) {
    return EMO_SPEAK;
  }
  if (!strcmp(name, "offline")) return EMO_OFFLINE;
  if (!strcmp(name, "quiet")) return EMO_QUIET;
  if (!strcmp(name, "silent")) return EMO_SILENT;
  if (!strcmp(name, "alarm")) return EMO_ALARM;
  if (!strcmp(name, "listen")) return EMO_LISTEN;
  if (!strcmp(name, "think")) return EMO_THINK;
  if (!strcmp(name, "speak")) return EMO_SPEAK;
  return EMO_SPEAK;
}

static bool httpJson(const char *method, const char *path, const String &body, JsonDocument &out, bool auth) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  HTTPClient http;
  String url = String(API_BASE) + path;
  http.setTimeout(12000);
  if (!http.begin(url)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  if (auth && token[0]) {
    http.addHeader("Authorization", String("Bearer ") + token);
  }

  int code = -1;
  if (!strcmp(method, "GET")) {
    code = http.GET();
  } else {
    code = http.POST(body);
  }

  String payload = http.getString();
  http.end();
  if (code < 200 || code >= 300) {
    Serial.printf("HTTP %s %s -> %d %s\n", method, path, code, payload.c_str());
    return false;
  }
  DeserializationError err = deserializeJson(out, payload);
  if (err) {
    Serial.printf("JSON err: %s\n", err.c_str());
    return false;
  }
  return true;
}

bool netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.printf("WiFi join %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  wifiOk = WiFi.status() == WL_CONNECTED;
  if (!wifiOk) {
    Serial.println("WiFi failed");
    return false;
  }
  Serial.print("WiFi OK ");
  Serial.println(WiFi.localIP());
  return netEnsureSession();
}

bool netReady() {
  return wifiOk && WiFi.status() == WL_CONNECTED && token[0] && sessionId[0];
}

bool netEnsureSession() {
  JsonDocument doc;
  String body = String("{\"username\":\"") + API_USER + "\",\"password\":\"" + API_PASS + "\"}";
  if (!httpJson("POST", "/v1/auth/login", body, doc, false)) {
    // Try register once for first boot of empty DB, then login again.
    String reg = String("{\"username\":\"") + API_USER + "\",\"password\":\"" + API_PASS +
                 "\",\"display_name\":\"Watch\"}";
    httpJson("POST", "/v1/auth/register", reg, doc, false);
    if (!httpJson("POST", "/v1/auth/login", body, doc, false)) {
      return false;
    }
  }
  const char *tok = doc["token"] | "";
  strncpy(token, tok, sizeof(token) - 1);
  token[sizeof(token) - 1] = 0;

  JsonDocument boot;
  String bootBody = String("{\"device_id\":\"") + DEVICE_ID + "\"}";
  if (!httpJson("POST", "/v1/watch/boot", bootBody, boot, true)) {
    // Fallback: open session directly
    JsonDocument sess;
    if (!httpJson("POST", "/v1/sessions", "{\"holder\":\"watch\",\"device_id\":\"" DEVICE_ID "\"}", sess, true)) {
      return false;
    }
    const char *sid = sess["session_id"] | "";
    strncpy(sessionId, sid, sizeof(sessionId) - 1);
  } else {
    const char *sid = boot["session_id"] | "";
    strncpy(sessionId, sid, sizeof(sessionId) - 1);
    const char *tok2 = boot["token"] | "";
    if (tok2[0]) {
      strncpy(token, tok2, sizeof(token) - 1);
    }
  }
  sessionId[sizeof(sessionId) - 1] = 0;
  Serial.printf("Session %s\n", sessionId);
  return sessionId[0] != 0;
}

CloudReply netTurn(const char *text) {
  CloudReply reply{};
  reply.emotion = EMO_OFFLINE;
  strncpy(reply.text, "No network", sizeof(reply.text) - 1);
  reply.ok = false;

  if (!netReady() && !netEnsureSession()) {
    return reply;
  }

  JsonDocument req;
  req["text"] = text ? text : "hi";
  String body;
  serializeJson(req, body);

  char path[64];
  snprintf(path, sizeof(path), "/v1/sessions/%s/turn", sessionId);

  JsonDocument doc;
  if (!httpJson("POST", path, body, doc, true)) {
    strncpy(reply.text, "Cloud error", sizeof(reply.text) - 1);
    return reply;
  }

  const char *emo = doc["emotion"] | "speak";
  const char *line = doc["subtitle"] | doc["text"] | "ok";
  reply.emotion = emotionFrom(emo);
  strncpy(reply.text, line, sizeof(reply.text) - 1);
  reply.text[sizeof(reply.text) - 1] = 0;
  reply.ok = true;
  return reply;
}

void netPrintStatus() {
  Serial.printf("WiFi=%s ip=%s token=%s session=%s\n",
                WiFi.status() == WL_CONNECTED ? "up" : "down",
                WiFi.localIP().toString().c_str(),
                token[0] ? "yes" : "no",
                sessionId[0] ? sessionId : "-");
}
