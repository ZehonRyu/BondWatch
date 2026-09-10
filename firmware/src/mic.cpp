#include "mic.h"
#include "pins.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

static bool ready = false;
static uint16_t lastRms = 0;

bool micBegin() {
  if (ready) {
    return true;
  }
  i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };
  i2s_pin_config_t pins = {
      .bck_io_num = PIN_MIC_SCK,
      .ws_io_num = PIN_MIC_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = PIN_MIC_SD,
  };
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("mic: i2s install fail");
    return false;
  }
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("mic: i2s pin fail");
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  ready = true;
  Serial.println("mic: I2S INMP441 ready");
  return true;
}

void micEnd() {
  if (!ready) {
    lastRms = 0;
    return;
  }
  i2s_driver_uninstall(I2S_NUM_0);
  ready = false;
  lastRms = 0;
}

uint16_t micLastRms() {
  return lastRms;
}

void micClearLevel() {
  lastRms = 0;
}

uint16_t micListenMs(unsigned ms) {
  if (!ready && !micBegin()) {
    return 0;
  }
  if (ms < 8) {
    ms = 8;
  }
  const unsigned long until = millis() + ms;
  double acc = 0;
  size_t samples = 0;
  int32_t buf[64];
  while (millis() < until) {
    size_t bytes = 0;
    // Short timeout so we do not stall the UI/touch loop if DMA is empty.
    if (i2s_read(I2S_NUM_0, buf, sizeof(buf), &bytes, 10) != ESP_OK || bytes == 0) {
      continue;
    }
    const size_t n = bytes / sizeof(int32_t);
    for (size_t i = 0; i < n; i++) {
      // INMP441: 24-bit left-justified in 32-bit word → use top 16 bits
      const int32_t s = buf[i] >> 16;
      acc += (double)(s * (double)s);
      samples++;
    }
  }
  if (samples == 0) {
    return lastRms;
  }
  const double rms = sqrt(acc / (double)samples);
  if (rms > 65535.0) {
    lastRms = 65535;
  } else {
    lastRms = static_cast<uint16_t>(rms);
  }
  return lastRms;
}

bool micVoiceDetected(uint16_t threshold) {
  // Always take a fresh short peek — never trust a leftover listen RMS.
  return micListenMs(12) >= threshold;
}
