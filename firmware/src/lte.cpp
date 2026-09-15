#include "lte.h"
#include "pins.h"

#include <Arduino.h>
#include <string.h>
#include <stdio.h>

static HardwareSerial &lteUart = Serial1;
static bool swapped = false;
static uint32_t baudNow = 115200;

static void uartStart(bool swap, uint32_t baud) {
  lteUart.end();
  delay(20);
  const int rx = swap ? PIN_4G_TX : PIN_4G_RX;
  const int tx = swap ? PIN_4G_RX : PIN_4G_TX;
  lteUart.setRxBufferSize(512);
  lteUart.begin(baud, SERIAL_8N1, rx, tx);
  swapped = swap;
  baudNow = baud;
  delay(50);
  Serial.printf("lte uart RX=%d TX=%d swap=%d baud=%u\n", rx, tx, swap ? 1 : 0, baud);
}

static void drain() {
  unsigned long t0 = millis();
  while (millis() - t0 < 40) {
    while (lteUart.available()) {
      lteUart.read();
    }
    delay(2);
  }
}

static void trimCopy(char *dst, size_t dstLen, const char *src) {
  if (!dst || dstLen == 0) {
    return;
  }
  dst[0] = 0;
  if (!src) {
    return;
  }
  while (*src == '\r' || *src == '\n' || *src == ' ') {
    src++;
  }
  size_t n = 0;
  while (src[n] && src[n] != '\r' && src[n] != '\n' && n + 1 < dstLen) {
    n++;
  }
  memcpy(dst, src, n);
  dst[n] = 0;
}

static bool atCmd(const char *cmd, char *out, size_t outLen, uint32_t timeoutMs) {
  if (out && outLen) {
    out[0] = 0;
  }
  drain();
  lteUart.print(cmd);
  lteUart.print("\r\n");
  size_t n = 0;
  const unsigned long t0 = millis();
  bool got = false;
  while (millis() - t0 < timeoutMs) {
    while (lteUart.available()) {
      const char c = static_cast<char>(lteUart.read());
      got = true;
      if (out && n + 1 < outLen) {
        out[n++] = c;
        out[n] = 0;
      }
      if (out && (strstr(out, "\nOK") || strstr(out, "\rOK") || strcmp(out, "OK") == 0 ||
                  strstr(out, "ERROR"))) {
        delay(15);
        while (lteUart.available() && out && n + 1 < outLen) {
          out[n++] = static_cast<char>(lteUart.read());
          out[n] = 0;
        }
        return strstr(out, "ERROR") == nullptr &&
               (strstr(out, "OK") != nullptr || strstr(out, "READY") != nullptr);
      }
    }
    delay(4);
  }
  (void)got;
  if (out && strstr(out, "OK")) {
    return true;
  }
  return false;
}

void lteBegin() {
  uartStart(false, 115200);
  drain();
}

void lteSelfTest(LteTestResult *out) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->csq = -1;
  strncpy(out->sim, "--", sizeof(out->sim) - 1);
  strncpy(out->id, "--", sizeof(out->id) - 1);
  strncpy(out->net, "--", sizeof(out->net) - 1);

  char buf[256];
  buf[0] = 0;
  bool atOk = false;
  const uint32_t bauds[] = {115200, 9600, 57600};
  for (uint32_t b : bauds) {
    for (int sw = 0; sw < 2; sw++) {
      uartStart(sw != 0, b);
      drain();
      atOk = atCmd("AT", buf, sizeof(buf), 600);
      Serial.printf("lte AT baud=%u swap=%d [%s]\n", b, sw, buf);
      if (atOk) {
        break;
      }
    }
    if (atOk) {
      break;
    }
  }
  out->gotBytes = buf[0] != 0;
  out->atOk = atOk;

  if (!atOk) {
    uartStart(false, 115200);
    if (!out->gotBytes) {
      strncpy(out->hint, "无应答 查交叉共地5V PK", sizeof(out->hint) - 1);
    } else {
      strncpy(out->hint, "有数据无OK 查波特率", sizeof(out->hint) - 1);
    }
    return;
  }

  if (atCmd("ATI", buf, sizeof(buf), 900)) {
    const char *p = strstr(buf, "Air");
    if (!p) {
      p = strstr(buf, "780");
    }
    if (!p) {
      // skip echo line
      p = strchr(buf, '\n');
      if (p) {
        p++;
      } else {
        p = buf;
      }
    }
    trimCopy(out->id, sizeof(out->id), p);
    if (!out->id[0]) {
      strncpy(out->id, "OK", sizeof(out->id) - 1);
    }
  }
  Serial.printf("lte ATI [%s]\n", buf);

  bool cpinReady = false;
  for (int i = 0; i < 4; i++) {
    atCmd("AT+CPIN?", buf, sizeof(buf), 900);
    Serial.printf("lte CPIN [%s]\n", buf);
    if (strstr(buf, "READY")) {
      strncpy(out->sim, "有卡", sizeof(out->sim) - 1);
      cpinReady = true;
      break;
    }
    if (strstr(buf, "SIM PIN") || strstr(buf, "SIM PUK")) {
      strncpy(out->sim, "锁卡", sizeof(out->sim) - 1);
      break;
    }
    if (i == 3) {
      if (strstr(buf, "ERROR") || strstr(buf, "CME")) {
        strncpy(out->sim, "无卡", sizeof(out->sim) - 1);
      } else {
        strncpy(out->sim, "未知", sizeof(out->sim) - 1);
      }
    } else {
      delay(700);
    }
  }

  for (int i = 0; i < 3; i++) {
    atCmd("AT+CSQ", buf, sizeof(buf), 800);
    Serial.printf("lte CSQ [%s]\n", buf);
    const char *cs = strstr(buf, "+CSQ:");
    if (cs) {
      int rssi = -1;
      int ber = 0;
      if (sscanf(cs, "+CSQ: %d,%d", &rssi, &ber) >= 1) {
        out->csq = static_cast<int16_t>(rssi);
      }
    }
    if (out->csq > 0 && out->csq <= 31) {
      break;
    }
    delay(400);
  }

  atCmd("AT+CREG?", buf, sizeof(buf), 800);
  Serial.printf("lte CREG [%s]\n", buf);
  int n = -1;
  int stat = -1;
  const char *cr = strstr(buf, "+CREG:");
  if (cr && sscanf(cr, "+CREG: %d,%d", &n, &stat) >= 2) {
    if (stat == 1 || stat == 5) {
      strncpy(out->net, "上网", sizeof(out->net) - 1);
    } else if (stat == 2) {
      strncpy(out->net, "搜网中", sizeof(out->net) - 1);
    } else {
      strncpy(out->net, "未上网", sizeof(out->net) - 1);
    }
  } else {
    strncpy(out->net, "--", sizeof(out->net) - 1);
  }

  atCmd("AT+COPS?", buf, sizeof(buf), 1200);
  Serial.printf("lte COPS [%s]\n", buf);

  if (strcmp(out->net, "上网") == 0 && out->csq >= 1 && out->csq <= 31) {
    strncpy(out->hint, "模组通 已上网", sizeof(out->hint) - 1);
  } else if (strcmp(out->net, "搜网中") == 0) {
    strncpy(out->hint, "有卡 正在搜网 可再测", sizeof(out->hint) - 1);
  } else if (cpinReady && (out->csq == 99 || out->csq == 0 || out->csq < 0)) {
    strncpy(out->hint, "有卡 无信号 查天线", sizeof(out->hint) - 1);
  } else if (strcmp(out->sim, "无卡") == 0) {
    strncpy(out->hint, "模组通 未插卡", sizeof(out->hint) - 1);
  } else if (strcmp(out->sim, "锁卡") == 0) {
    strncpy(out->hint, "卡要PIN", sizeof(out->hint) - 1);
  } else {
    strncpy(out->hint, "模组通", sizeof(out->hint) - 1);
  }
  if (swapped) {
    size_t n = strlen(out->hint);
    strncpy(out->hint + n, " 线已对调", sizeof(out->hint) - n - 1);
  }
}
