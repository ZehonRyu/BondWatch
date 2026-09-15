#include "net.h"
#include "secrets.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <time.h>

enum { ST_IDLE = 0, ST_WAIT, ST_SESS, ST_OK, ST_RECONN, ST_API_WAIT };

static uint8_t st = ST_IDLE;
static unsigned long stAt = 0;
static char token[80] = {0};
static char sessionId[24] = {0};
static char pairCode[12] = {0};
static char linkLine[40] = "WiFi...";
static char faceEmo[16] = "idle";
static char faceText[64] = {0};
static bool wifiOk = false;
static bool faceDirty = false;
static uint32_t lastFaceSeq = 0;
static unsigned long lastPubAt = 0;
static unsigned long lastPollAt = 0;
static unsigned long lastNtpAt = 0;
static NetFaceHandler remoteFace = nullptr;

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
  return EMO_IDLE;
}

static void setLink(const char *line) {
  strncpy(linkLine, line ? line : "", sizeof(linkLine) - 1);
  linkLine[sizeof(linkLine) - 1] = 0;
}

static void rememberPair(JsonDocument &doc) {
  const char *pc = doc["pair_code"] | "";
  if (pc[0]) {
    strncpy(pairCode, pc, sizeof(pairCode) - 1);
    pairCode[sizeof(pairCode) - 1] = 0;
  }
}

static bool httpJson(const char *method, const char *path, const String &body, JsonDocument &out, bool auth) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  HTTPClient http;
  String url = String(API_BASE) + path;
  http.setTimeout(1200);
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

static void startNtp() {
  configTzTime("CST-8", "ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org");
  lastNtpAt = millis();
  Serial.println("NTP Beijing CST-8");
}

void netOnRemoteFace(NetFaceHandler cb) { remoteFace = cb; }

static void startWifiJoin(bool clearSession) {
  if (clearSession) {
    token[0] = 0;
    sessionId[0] = 0;
  }
  wifiOk = false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  st = ST_WAIT;
  stAt = millis();
  setLink("WiFi...");
}

void netBegin() {
  Serial.printf("WiFi join %s ...\n", WIFI_SSID);
  startWifiJoin(true);
}

static void startWifiReconnect() {
  Serial.println("WiFi reconnect (keep session)");
  wifiOk = false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.reconnect();
  st = ST_RECONN;
  stAt = millis();
  setLink("WiFi...");
}

static void setPairLink() {
  if (pairCode[0]) {
    char line[32];
    snprintf(line, sizeof(line), "PAIR %s", pairCode);
    setLink(line);
  } else if (sessionId[0]) {
    setLink("BOUND");
  }
}

bool netWifiOk() { return WiFi.status() == WL_CONNECTED; }

bool netReady() { return wifiOk && WiFi.status() == WL_CONNECTED && token[0] && sessionId[0]; }

bool netCloudUp() { return st == ST_OK && WiFi.status() == WL_CONNECTED; }

bool netCanTalk() { return netCloudUp() && token[0] && sessionId[0]; }

bool netBound() { return netReady() && pairCode[0]; }

const char *netPairCode() { return pairCode[0] ? pairCode : "----"; }

const char *netLinkLine() { return linkLine; }

void netPublishFace(const char *emotion, const char *text) {
  strncpy(faceEmo, emotion ? emotion : "idle", sizeof(faceEmo) - 1);
  faceEmo[sizeof(faceEmo) - 1] = 0;
  strncpy(faceText, text ? text : "", sizeof(faceText) - 1);
  faceText[sizeof(faceText) - 1] = 0;
  faceDirty = true;
}

bool netEnsureSession() {
  JsonDocument doc;
  String body = String("{\"username\":\"") + API_USER + "\",\"password\":\"" + API_PASS + "\"}";
  if (!httpJson("POST", "/v1/auth/login", body, doc, false)) {
    String reg = String("{\"username\":\"") + API_USER + "\",\"password\":\"" + API_PASS +
                 "\",\"display_name\":\"Watch\"}";
    httpJson("POST", "/v1/auth/register", reg, doc, false);
    if (!httpJson("POST", "/v1/auth/login", body, doc, false)) {
      return false;
    }
  }
  rememberPair(doc);
  const char *tok = doc["token"] | "";
  strncpy(token, tok, sizeof(token) - 1);
  token[sizeof(token) - 1] = 0;

  JsonDocument boot;
  String bootBody = String("{\"device_id\":\"") + DEVICE_ID + "\",\"name\":\"BondWatch\"}";
  if (!httpJson("POST", "/v1/watch/boot", bootBody, boot, true)) {
    JsonDocument sess;
    if (!httpJson("POST", "/v1/sessions", "{\"holder\":\"watch\",\"device_id\":\"" DEVICE_ID "\"}", sess, true)) {
      return false;
    }
    const char *sid = sess["session_id"] | "";
    strncpy(sessionId, sid, sizeof(sessionId) - 1);
  } else {
    rememberPair(boot);
    const char *sid = boot["session_id"] | "";
    strncpy(sessionId, sid, sizeof(sessionId) - 1);
    const char *tok2 = boot["token"] | "";
    if (tok2[0]) {
      strncpy(token, tok2, sizeof(token) - 1);
    }
  }
  sessionId[sizeof(sessionId) - 1] = 0;
  Serial.printf("Session %s pair=%s\n", sessionId, pairCode);
  return sessionId[0] != 0;
}

static bool publishNow() {
  JsonDocument req;
  req["device_id"] = DEVICE_ID;
  req["emotion"] = faceEmo;
  req["text"] = faceText;
  req["source"] = "watch";
  req["wifi_ok"] = true;
  String body;
  serializeJson(req, body);
  JsonDocument doc;
  if (!httpJson("POST", "/v1/watch/face", body, doc, true)) {
    return false;
  }
  lastFaceSeq = doc["seq"] | lastFaceSeq;
  faceDirty = false;
  return true;
}

static void pollRemote() {
  JsonDocument doc;
  if (!httpJson("GET", "/v1/watch/face", "", doc, true)) {
    return;
  }
  const uint32_t seq = doc["seq"] | 0;
  const char *src = doc["source"] | "";
  const char *emo = doc["emotion"] | "speak";
  const char *text = doc["text"] | "";
  rememberPair(doc);
  if (seq <= lastFaceSeq || !strcmp(src, "watch") || !text[0]) {
    if (seq > lastFaceSeq) {
      lastFaceSeq = seq;
    }
    return;
  }
  lastFaceSeq = seq;
  if (remoteFace) {
    remoteFace(emo, text, src);
  }
}

void netTick() {
  const unsigned long now = millis();
  if (st == ST_WAIT) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiOk = true;
      st = ST_SESS;
      Serial.print("WiFi OK ");
      Serial.println(WiFi.localIP());
      startNtp();
      setLink("API...");
    } else if (now - stAt > 25000UL) {
      wifiOk = false;
      st = ST_IDLE;
      stAt = now;
      setLink("WiFi fail");
      Serial.println("WiFi failed, retry later");
    }
    return;
  }

  if (st == ST_RECONN) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiOk = true;
      Serial.print("WiFi back ");
      Serial.println(WiFi.localIP());
      if (sessionId[0] && token[0]) {
        st = ST_OK;
        setPairLink();
      } else {
        st = ST_SESS;
        setLink("API...");
      }
    } else if (now - stAt > 15000UL) {
      stAt = now;
      WiFi.reconnect();
      Serial.println("WiFi reconnect retry");
    }
    return;
  }

  if (st == ST_API_WAIT) {
    if (WiFi.status() != WL_CONNECTED) {
      startWifiReconnect();
      return;
    }
    if (now - stAt > 12000UL) {
      stAt = now;
      if (netEnsureSession()) {
        st = ST_OK;
        faceDirty = true;
        setPairLink();
        Serial.println("API back");
      } else {
        Serial.println("API retry fail");
      }
    }
    return;
  }

  if (st == ST_IDLE && now - stAt > 20000UL) {
    if (token[0] || sessionId[0]) {
      startWifiReconnect();
    } else {
      netBegin();
    }
    return;
  }

  if (st == ST_SESS) {
    if (netEnsureSession()) {
      st = ST_OK;
      faceDirty = true;
      setPairLink();
    } else {
      stAt = now;
      if (WiFi.status() == WL_CONNECTED) {
        st = ST_API_WAIT;
        setLink("API fail");
      } else {
        startWifiReconnect();
      }
    }
    return;
  }

  if (st != ST_OK) {
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    startWifiReconnect();
    Serial.println("WiFi lost, reconnecting");
    return;
  }
  setPairLink();

  if (faceDirty && now - lastPubAt > 350UL) {
    lastPubAt = now;
    if (!publishNow()) {
      Serial.println("face publish fail");
    }
    return;
  }
  if (now - lastPollAt > 2000UL) {
    lastPollAt = now;
    pollRemote();
  }
  if (now - lastNtpAt > 6UL * 60UL * 60UL * 1000UL) {
    startNtp();
  }
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

static void wavU32(uint8_t *p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16);
  p[3] = static_cast<uint8_t>(v >> 24);
}

static void wavU16(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}

bool netUploadWav(const int16_t *pcm, size_t samples) {
  if (!pcm || samples < 160) {
    return false;
  }
  if (!netCanTalk()) {
    Serial.println("wav: cloud down");
    return false;
  }
  const uint32_t dataBytes = static_cast<uint32_t>(samples * sizeof(int16_t));
  const uint32_t total = 44 + dataBytes;
  uint8_t *wav = static_cast<uint8_t *>(heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!wav) {
    wav = static_cast<uint8_t *>(malloc(total));
  }
  if (!wav) {
    Serial.println("wav: oom");
    return false;
  }
  memcpy(wav, "RIFF", 4);
  wavU32(wav + 4, 36 + dataBytes);
  memcpy(wav + 8, "WAVEfmt ", 8);
  wavU32(wav + 16, 16);
  wavU16(wav + 20, 1);
  wavU16(wav + 22, 1);
  wavU32(wav + 24, 16000);
  wavU32(wav + 28, 32000);
  wavU16(wav + 32, 2);
  wavU16(wav + 34, 16);
  memcpy(wav + 36, "data", 4);
  wavU32(wav + 40, dataBytes);
  memcpy(wav + 44, pcm, dataBytes);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("wav: wifi down");
    free(wav);
    return false;
  }

  HTTPClient http;
  String url = String(API_BASE) + "/v1/watch/audio";
  http.setTimeout(3000);
  bool ok = false;
  if (http.begin(url)) {
    http.addHeader("Content-Type", "audio/wav");
    if (token[0]) {
      http.addHeader("Authorization", String("Bearer ") + token);
    }
    const int code = http.POST(wav, total);
    String payload = http.getString();
    http.end();
    Serial.printf("wav POST %d bytes=%u %s\n", code, static_cast<unsigned>(total), payload.c_str());
    ok = code >= 200 && code < 300;
  }
  free(wav);
  return ok;
}

void netPrintStatus() {
  Serial.printf("WiFi=%s ip=%s token=%s session=%s pair=%s\n",
                WiFi.status() == WL_CONNECTED ? "up" : "down",
                WiFi.localIP().toString().c_str(),
                token[0] ? "yes" : "no",
                sessionId[0] ? sessionId : "-",
                pairCode[0] ? pairCode : "-");
}
