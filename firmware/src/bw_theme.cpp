#include "bw_theme.h"

extern const lv_font_t font_cn_16;

namespace bw {

void styleScreenPastel(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(kPastelSky), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(kPastelSun), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

void styleScreenDark(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(kScreenDark), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

lv_obj_t *makeStatusBar(lv_obj_t *parent, lv_obj_t **outTime, lv_obj_t **outWifiArc) {
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_set_size(bar, 240, 36);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(kStatusBar), 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *time = lv_label_create(bar);
  lv_label_set_text(time, "08:00");
  lv_obj_set_style_text_font(time, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(time, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(time, LV_ALIGN_LEFT_MID, 12, 0);

  lv_obj_t *arc = lv_arc_create(bar);
  lv_obj_set_size(arc, 26, 26);
  lv_obj_align(arc, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_arc_set_rotation(arc, 270);
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_arc_set_angles(arc, 0, 300);
  lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_hex(kWifiGreen), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);

  if (outTime) {
    *outTime = time;
  }
  if (outWifiArc) {
    *outWifiArc = arc;
  }
  return bar;
}

lv_obj_t *makeCard(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_coord_t y) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(kCardCream), 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 12, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
  lv_obj_set_style_shadow_ofs_y(card, 4, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

lv_obj_t *makeGlowRing(lv_obj_t *parent, lv_coord_t size, lv_opa_t opa) {
  lv_obj_t *ring = lv_obj_create(parent);
  lv_obj_set_size(ring, size, size);
  lv_obj_align(ring, LV_ALIGN_CENTER, 0, -8);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(ring, LV_OPA_0, 0);
  lv_obj_set_style_border_color(ring, lv_color_hex(kGlowA), 0);
  lv_obj_set_style_border_width(ring, 2, 0);
  lv_obj_set_style_border_opa(ring, opa, 0);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  return ring;
}

lv_obj_t *makePillBtn(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_coord_t y, uint32_t bg) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_radius(btn, h / 2, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
  lv_obj_set_style_shadow_width(btn, 8, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
  lv_obj_set_style_shadow_ofs_y(btn, 2, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  return btn;
}

lv_obj_t *makePageDots(lv_obj_t *parent, lv_obj_t *dots[5], uint8_t active) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, 56, 12);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 252);
  lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  for (uint8_t i = 0; i < 5; ++i) {
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_set_size(dot, i == active ? 8 : 6, i == active ? 8 : 6);
    lv_obj_set_pos(dot, static_cast<lv_coord_t>(i * 12), i == active ? 2 : 3);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(i == active ? kGlowA : 0x555555), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    if (dots) {
      dots[i] = dot;
    }
  }
  return row;
}

lv_obj_t *makeNavRow(lv_obj_t *parent, lv_coord_t y, uint32_t accent, const char *name, lv_event_cb_t cb) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, 216, 62);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_radius(row, 20, 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(kRowDark), 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *chip = lv_obj_create(row);
  lv_obj_set_size(chip, 36, 36);
  lv_obj_align(chip, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(chip, lv_color_hex(accent), 0);
  lv_obj_set_style_border_width(chip, 0, 0);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *lab = lv_label_create(row);
  lv_label_set_text(lab, name);
  lv_obj_set_style_text_font(lab, &font_cn_16, 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(lab, LV_ALIGN_LEFT_MID, 56, 0);

  lv_obj_t *go = lv_label_create(row);
  lv_label_set_text(go, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(go, lv_color_hex(kMuted), 0);
  lv_obj_align(go, LV_ALIGN_RIGHT_MID, -12, 0);
  return row;
}

lv_obj_t *makeSettingRow(lv_obj_t *parent, lv_coord_t y, const char *name) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, 216, 56);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_radius(row, 18, 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(kRowDark), 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *n = lv_label_create(row);
  lv_label_set_text(n, name);
  lv_obj_set_style_text_font(n, &font_cn_16, 0);
  lv_obj_set_style_text_color(n, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(n, LV_ALIGN_LEFT_MID, 12, 0);
  return row;
}

void styleBackBtn(lv_obj_t *btn) {
  lv_obj_set_size(btn, 76, 40);
  lv_obj_set_style_radius(btn, 20, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(kPillDark), 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_pad_hor(btn, 10, 0);
}

void styleSlider(lv_obj_t *slider) {
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_hex(kGlowA), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
}

}  // namespace bw
