#pragma once
#include <stdint.h>

#define LUMI_FRAME_W 160
#define LUMI_FRAME_H 176
#define LUMI_FRAME_N 15
#define LUMI_IDLE_N 12
#define LUMI_LISTEN_OFF 12
#define LUMI_LISTEN_N 3
#define LUMI_FRAME_MS 120
#define LUMI_FRAME_BYTES (160 * 176 * 2)
extern const uint8_t lumi_frames[];
