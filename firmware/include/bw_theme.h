#pragma once

#include <lvgl.h>

namespace bw {

constexpr uint32_t kPastelSky = 0xC5E3F6;
constexpr uint32_t kPastelSun = 0xFFF3C4;
constexpr uint32_t kCardCream = 0xFFF8EE;
constexpr uint32_t kInk = 0x1A1A1A;
constexpr uint32_t kMuted = 0x8A8A8A;
constexpr uint32_t kStatusBar = 0x000000;
constexpr uint32_t kWifiGreen = 0x39D353;
constexpr uint32_t kGlowA = 0xFFB300;
constexpr uint32_t kGlowB = 0xFF8F00;
constexpr uint32_t kPillCoral = 0xFF9F43;
constexpr uint32_t kPillDark = 0x2A2A2E;
constexpr uint32_t kRowDark = 0x1C1C1E;
constexpr uint32_t kScreenDark = 0x050505;
constexpr uint32_t kAccentBlue = 0x5EB8FF;
constexpr uint32_t kAccentPurple = 0x9B7BFF;
constexpr uint32_t kAccentMint = 0x3DDC97;

void styleScreenPastel(lv_obj_t *scr);
void styleScreenDark(lv_obj_t *scr);
lv_obj_t *makeStatusBar(lv_obj_t *parent, lv_obj_t **outTime, lv_obj_t **outWifiArc);
lv_obj_t *makeCard(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_coord_t y);
lv_obj_t *makeGlowRing(lv_obj_t *parent, lv_coord_t size, lv_opa_t opa);
lv_obj_t *makePillBtn(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_coord_t y, uint32_t bg);
lv_obj_t *makePageDots(lv_obj_t *parent, lv_obj_t *dots[5], uint8_t active);
lv_obj_t *makeNavRow(lv_obj_t *parent, lv_coord_t y, uint32_t accent, const char *name, lv_event_cb_t cb);
lv_obj_t *makeSettingRow(lv_obj_t *parent, lv_coord_t y, const char *name);
void styleBackBtn(lv_obj_t *btn);
void styleSlider(lv_obj_t *slider);

}  // namespace bw
