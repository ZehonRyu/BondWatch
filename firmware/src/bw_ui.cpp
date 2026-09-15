#include "bw_ui.h"
#include "bw_theme.h"
#include "lv_port.h"
#include "prefs.h"
#include "lang.h"
#include "ui.h"
#include "app.h"
#include "touch.h"
#include "lumi_frames.h"

#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *scrHome = nullptr;
static lv_obj_t *scrMenu = nullptr;
static lv_obj_t *scrCtrl = nullptr;
static lv_obj_t *scrSet = nullptr;
static lv_obj_t *scrCal = nullptr;
static lv_obj_t *scrLte = nullptr;

static lv_obj_t *labTime = nullptr;
static lv_obj_t *labSub = nullptr;
static lv_obj_t *labVer = nullptr;
static lv_obj_t *labHint = nullptr;
static lv_obj_t *labPair = nullptr;
static lv_obj_t *objStatusBar = nullptr;
static lv_obj_t *objCard = nullptr;
static lv_obj_t *objGlow[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *objVuBg = nullptr;
static lv_obj_t *objVuFill = nullptr;
static lv_obj_t *btnTalk = nullptr;
static lv_obj_t *labTalk = nullptr;
static bool recOn = false;
static bool listenMode = false;
static uint8_t idleFrame = 0;
static uint8_t listenFrame = 0;
#ifndef LUMI_IDLE_N
#define LUMI_IDLE_N LUMI_FRAME_N
#endif
#ifndef LUMI_LISTEN_OFF
#define LUMI_LISTEN_OFF LUMI_IDLE_N
#endif
#ifndef LUMI_LISTEN_N
#define LUMI_LISTEN_N 1
#endif
static lv_obj_t *objFace = nullptr;
static lv_obj_t *imgLumi = nullptr;
static lv_img_dsc_t lumiDsc;
static uint8_t lumiIdx = 0;

static lv_obj_t *swDnd = nullptr;
static lv_obj_t *slBright = nullptr;
static lv_obj_t *slVol = nullptr;
static lv_obj_t *labBright = nullptr;
static lv_obj_t *labVol = nullptr;

static lv_obj_t *swLand = nullptr;
static lv_obj_t *labLang = nullptr;
static lv_obj_t *labTouch = nullptr;

static lv_obj_t *labCal = nullptr;
static lv_obj_t *btnCalA = nullptr;
static lv_obj_t *btnCalB = nullptr;
static lv_obj_t *btnCalDone = nullptr;

static lv_obj_t *labLteStatus = nullptr;
static lv_obj_t *labLteId = nullptr;
static lv_obj_t *labLteSim = nullptr;
static lv_obj_t *labLteCsq = nullptr;
static lv_obj_t *labLteHint = nullptr;

static const lv_font_t *cn() { return &font_cn_16; }

static void styleLabelCn(lv_obj_t *lab, uint32_t color) {
  lv_obj_set_style_text_font(lab, cn(), 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(color), 0);
}

static void lumiSetFrame(uint8_t i);

static void applyHomeTheme(bool listen) {
  if (!scrHome) {
    return;
  }
  listenMode = listen;
  if (listen) {
    bw::styleScreenDark(scrHome);
    if (objCard) {
      lv_obj_set_style_bg_opa(objCard, LV_OPA_0, 0);
      lv_obj_set_style_shadow_width(objCard, 0, 0);
    }
    for (int i = 0; i < 3; ++i) {
      if (objGlow[i]) {
        lv_obj_clear_flag(objGlow[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (labSub) {
      lv_obj_set_style_text_color(labSub, lv_color_hex(0xFFCC80), 0);
    }
    listenFrame = 0;
    if (LUMI_LISTEN_N > 0) {
      lumiSetFrame(static_cast<uint8_t>(LUMI_LISTEN_OFF));
    }
  } else {
    bw::styleScreenPastel(scrHome);
    if (objCard) {
      lv_obj_set_style_bg_color(objCard, lv_color_hex(bw::kCardCream), 0);
      lv_obj_set_style_bg_opa(objCard, LV_OPA_COVER, 0);
      lv_obj_set_style_shadow_width(objCard, 12, 0);
    }
    for (int i = 0; i < 3; ++i) {
      if (objGlow[i]) {
        lv_obj_add_flag(objGlow[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (labSub) {
      lv_obj_set_style_text_color(labSub, lv_color_hex(bw::kMuted), 0);
    }
    idleFrame = 0;
    lumiSetFrame(0);
  }
}

static void loadScr(lv_obj_t *scr, bool forward) {
  if (lv_scr_act() == scr) {
    return;
  }
  lv_scr_load_anim(scr, forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT, 140, 0, false);
}

static void lumiSetFrame(uint8_t i) {
  lumiIdx = static_cast<uint8_t>(i % LUMI_FRAME_N);
  lumiDsc.header.always_zero = 0;
  lumiDsc.header.w = LUMI_FRAME_W;
  lumiDsc.header.h = LUMI_FRAME_H;
  lumiDsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  lumiDsc.data_size = LUMI_FRAME_BYTES;
  lumiDsc.data = lumi_frames + static_cast<uint32_t>(lumiIdx) * LUMI_FRAME_BYTES;
  if (imgLumi) {
    lv_img_cache_invalidate_src(&lumiDsc);
    lv_img_set_src(imgLumi, &lumiDsc);
  }
}

static void evBackBtn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  touchSuppressMs(500);
  appGoHome();
}

static lv_obj_t *addBackBtn(lv_obj_t *parent) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_align(b, LV_ALIGN_TOP_LEFT, 6, 6);
  bw::styleBackBtn(b);
  lv_obj_set_ext_click_area(b, 12);
  lv_obj_add_flag(b, LV_OBJ_FLAG_FLOATING);
  lv_obj_move_foreground(b);
  lv_obj_add_event_cb(b, evBackBtn, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, tr("返回", "Back"));
  styleLabelCn(l, 0xFFFFFF);
  lv_obj_center(l);
  lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
  return b;
}

static void makeTitle(lv_obj_t *parent, const char *title, uint32_t textColor) {
  lv_obj_t *back = addBackBtn(parent);

  lv_obj_t *t = lv_label_create(parent);
  lv_label_set_text(t, title);
  styleLabelCn(t, textColor);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);
  lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *v = lv_label_create(parent);
  lv_label_set_text(v, BW_VERSION);
  lv_obj_set_style_text_color(v, lv_color_hex(bw::kAccentBlue), 0);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
  lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -10, 14);
  lv_obj_clear_flag(v, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_move_foreground(back);
}

static void evOpenMenu(lv_event_t *e) {
  (void)e;
  touchSuppressMs(500);
  appOpenMenu();
}

static void evOpenCtrl(lv_event_t *e) {
  (void)e;
  touchSuppressMs(500);
  appOpenControl();
}

static void evOpenSet(lv_event_t *e) {
  (void)e;
  touchSuppressMs(500);
  appOpenSettings();
}

static void evOpenCal(lv_event_t *e) {
  (void)e;
  touchSuppressMs(500);
  appOpenTouchCal();
}

static void evOpenLte(lv_event_t *e) {
  (void)e;
  appOpenLteTest();
}

static void evStartTalk(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("talk btn");
    appStartTalk();
  }
}

static void evMicTx(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    appRunMicTx();
  }
}

static void evLteRetry(lv_event_t *e) {
  (void)e;
  appOpenLteTest();
}

static void evBackToSet(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  touchSuppressMs(500);
  appOpenSettings();
}

static void evDnd(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  prefs().dnd = lv_obj_has_state(swDnd, LV_STATE_CHECKED);
  prefsSave();
}

static void evBright(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  prefs().brightness = static_cast<uint8_t>(lv_slider_get_value(slBright));
  uiSetBrightnessLevel(prefs().brightness);
  prefsSave();
  const char *n[] = {"低", "中", "高"};
  if (labBright) {
    lv_label_set_text(labBright, n[prefs().brightness % 3]);
  }
}

static void evVol(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  prefs().volume = static_cast<uint8_t>(lv_slider_get_value(slVol));
  prefsSave();
  const char *n[] = {"静音", "低", "中", "高"};
  if (labVol) {
    lv_label_set_text(labVol, n[prefs().volume % 4]);
  }
}

static void evLand(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  prefs().landscape = lv_obj_has_state(swLand, LV_STATE_CHECKED);
  prefsSave();
}

static void evLang(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  prefsCycleLang();
  if (labLang) {
    lv_label_set_text(labLang, langIsEn() ? "English" : "中文");
  }
}

static void evCalA(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    touchSuppressMs(500);
    TouchGesture g{};
    g.type = TG_TAP;
    g.x = 40;
    g.y = 80;
    appHandleGesture(g);
  }
}

static void evCalB(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    touchSuppressMs(500);
    TouchGesture g{};
    g.type = TG_TAP;
    g.x = 200;
    g.y = 240;
    appHandleGesture(g);
  }
}

static void evCalDone(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    touchSuppressMs(500);
    TouchGesture g{};
    g.type = TG_TAP;
    g.x = 120;
    g.y = 300;
    appHandleGesture(g);
  }
}

static void buildHome() {
  scrHome = lv_obj_create(nullptr);
  bw::styleScreenPastel(scrHome);
  lv_obj_clear_flag(scrHome, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(scrHome, LV_OBJ_FLAG_GESTURE_BUBBLE);

  objStatusBar = bw::makeStatusBar(scrHome, &labTime, nullptr);

  objCard = bw::makeCard(scrHome, 216, 196, 42);
  objGlow[0] = bw::makeGlowRing(objCard, 150, LV_OPA_20);
  objGlow[1] = bw::makeGlowRing(objCard, 120, LV_OPA_40);
  objGlow[2] = bw::makeGlowRing(objCard, 90, LV_OPA_60);
  for (int i = 0; i < 3; ++i) {
    lv_obj_add_flag(objGlow[i], LV_OBJ_FLAG_HIDDEN);
  }

  imgLumi = lv_img_create(objCard);
  lumiSetFrame(0);
  lv_img_set_src(imgLumi, &lumiDsc);
  lv_obj_set_size(imgLumi, LUMI_FRAME_W, LUMI_FRAME_H);
  lv_obj_align(imgLumi, LV_ALIGN_CENTER, 0, -10);
  lv_obj_clear_flag(imgLumi, LV_OBJ_FLAG_CLICKABLE);
  objFace = imgLumi;

  labPair = lv_label_create(objCard);
  lv_label_set_text(labPair, "PAIR ----");
  lv_obj_set_style_text_font(labPair, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(labPair, lv_color_hex(bw::kMuted), 0);
  lv_obj_align(labPair, LV_ALIGN_TOP_RIGHT, -8, 6);

  labSub = lv_label_create(objCard);
  lv_label_set_text(labSub, tr("待机", "idle"));
  styleLabelCn(labSub, bw::kMuted);
  lv_obj_set_width(labSub, 190);
  lv_label_set_long_mode(labSub, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(labSub, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(labSub, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_clear_flag(labSub, LV_OBJ_FLAG_CLICKABLE);

  labHint = lv_label_create(scrHome);
  lv_label_set_text(labHint, tr("左右滑菜单 · 上下滑控制", "swipe nav"));
  styleLabelCn(labHint, bw::kMuted);
  lv_obj_align(labHint, LV_ALIGN_TOP_MID, 0, 246);
  lv_obj_set_style_text_opa(labHint, LV_OPA_60, 0);

  bw::makePageDots(scrHome, nullptr, 2);

  btnTalk = bw::makePillBtn(scrHome, 200, 36, 0, bw::kPillCoral);
  lv_obj_align(btnTalk, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_flag(btnTalk, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(btnTalk, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(btnTalk, evStartTalk, LV_EVENT_CLICKED, nullptr);
  labTalk = lv_label_create(btnTalk);
  lv_label_set_text(labTalk, tr("开始对话", "Talk"));
  styleLabelCn(labTalk, bw::kInk);
  lv_obj_center(labTalk);
  lv_obj_clear_flag(labTalk, LV_OBJ_FLAG_CLICKABLE);

  objVuBg = lv_obj_create(scrHome);
  lv_obj_set_size(objVuBg, 180, 8);
  lv_obj_align_to(objVuBg, btnTalk, LV_ALIGN_OUT_TOP_MID, 0, -6);
  lv_obj_clear_flag(objVuBg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(objVuBg, 4, 0);
  lv_obj_set_style_bg_color(objVuBg, lv_color_hex(0x333333), 0);
  lv_obj_set_style_border_width(objVuBg, 0, 0);
  lv_obj_set_style_pad_all(objVuBg, 1, 0);
  lv_obj_add_flag(objVuBg, LV_OBJ_FLAG_HIDDEN);

  objVuFill = lv_obj_create(objVuBg);
  lv_obj_set_size(objVuFill, 8, 6);
  lv_obj_align(objVuFill, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_radius(objVuFill, 3, 0);
  lv_obj_set_style_bg_color(objVuFill, lv_color_hex(bw::kWifiGreen), 0);
  lv_obj_set_style_border_width(objVuFill, 0, 0);
  lv_obj_clear_flag(objVuFill, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_move_foreground(btnTalk);
  lv_obj_move_foreground(objStatusBar);
  lv_obj_move_foreground(imgLumi);

  labVer = lv_label_create(scrHome);
  lv_label_set_text(labVer, BW_VERSION);
  lv_obj_set_style_text_color(labVer, lv_color_hex(bw::kMuted), 0);
  lv_obj_set_style_text_font(labVer, &lv_font_montserrat_14, 0);
  lv_obj_align(labVer, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
  lv_obj_set_style_text_opa(labVer, LV_OPA_50, 0);
}

static void buildMenu() {
  scrMenu = lv_obj_create(nullptr);
  bw::styleScreenPastel(scrMenu);
  lv_obj_set_scroll_dir(scrMenu, LV_DIR_VER);
  makeTitle(scrMenu, tr("菜单", "Menu"), bw::kInk);

  bw::makeNavRow(scrMenu, 52, bw::kAccentPurple, tr("控制", "Control"), evOpenCtrl);
  bw::makeNavRow(scrMenu, 122, bw::kAccentBlue, tr("设置", "Settings"), evOpenSet);
  bw::makeNavRow(scrMenu, 192, bw::kAccentMint, tr("触摸校准", "Touch cal"), evOpenCal);
}

static void buildCtrl() {
  scrCtrl = lv_obj_create(nullptr);
  bw::styleScreenDark(scrCtrl);
  lv_obj_clear_flag(scrCtrl, LV_OBJ_FLAG_SCROLLABLE);
  makeTitle(scrCtrl, tr("控制", "Control"), 0xFFFFFF);

  lv_obj_t *row = bw::makeSettingRow(scrCtrl, 52, tr("勿扰", "DND"));
  styleLabelCn(lv_obj_get_child(row, 0), 0xFFFFFF);
  swDnd = lv_switch_create(row);
  lv_obj_align(swDnd, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_add_event_cb(swDnd, evDnd, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *lb = lv_label_create(scrCtrl);
  lv_label_set_text(lb, tr("亮度", "Bright"));
  styleLabelCn(lb, 0xFFFFFF);
  lv_obj_align(lb, LV_ALIGN_TOP_LEFT, 16, 124);
  labBright = lv_label_create(scrCtrl);
  lv_label_set_text(labBright, "中");
  styleLabelCn(labBright, bw::kGlowA);
  lv_obj_align(labBright, LV_ALIGN_TOP_RIGHT, -16, 124);
  slBright = lv_slider_create(scrCtrl);
  lv_obj_set_width(slBright, 208);
  lv_slider_set_range(slBright, 0, 2);
  lv_obj_align(slBright, LV_ALIGN_TOP_MID, 0, 152);
  bw::styleSlider(slBright);
  lv_obj_add_event_cb(slBright, evBright, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *lv = lv_label_create(scrCtrl);
  lv_label_set_text(lv, tr("声音", "Volume"));
  styleLabelCn(lv, 0xFFFFFF);
  lv_obj_align(lv, LV_ALIGN_TOP_LEFT, 16, 200);
  labVol = lv_label_create(scrCtrl);
  lv_label_set_text(labVol, "中");
  styleLabelCn(labVol, bw::kGlowA);
  lv_obj_align(labVol, LV_ALIGN_TOP_RIGHT, -16, 200);
  slVol = lv_slider_create(scrCtrl);
  lv_obj_set_width(slVol, 208);
  lv_slider_set_range(slVol, 0, 3);
  lv_obj_align(slVol, LV_ALIGN_TOP_MID, 0, 228);
  bw::styleSlider(slVol);
  lv_obj_add_event_cb(slVol, evVol, LV_EVENT_VALUE_CHANGED, nullptr);
}

static lv_obj_t *setRow(lv_obj_t *parent, int16_t y, const char *name) {
  lv_obj_t *row = bw::makeSettingRow(parent, y, name);
  styleLabelCn(lv_obj_get_child(row, 0), 0xFFFFFF);
  return row;
}

static void buildSet() {
  scrSet = lv_obj_create(nullptr);
  bw::styleScreenPastel(scrSet);
  lv_obj_set_scroll_dir(scrSet, LV_DIR_VER);
  makeTitle(scrSet, tr("设置", "Settings"), bw::kInk);

  lv_obj_t *rLte = setRow(scrSet, 50, tr("4G自测", "4G test"));
  lv_obj_add_event_cb(rLte, evOpenLte, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *goL = lv_label_create(rLte);
  lv_label_set_text(goL, ">");
  lv_obj_align(goL, LV_ALIGN_RIGHT_MID, -8, 0);

  lv_obj_t *rMic = setRow(scrSet, 116, "MIC TX");
  lv_obj_add_event_cb(rMic, evMicTx, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *goM = lv_label_create(rMic);
  lv_label_set_text(goM, ">");
  lv_obj_align(goM, LV_ALIGN_RIGHT_MID, -8, 0);

  lv_obj_t *r0 = setRow(scrSet, 182, tr("触摸校准", "Touch cal"));
  lv_obj_add_event_cb(r0, evOpenCal, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *go = lv_label_create(r0);
  lv_label_set_text(go, ">");
  lv_obj_align(go, LV_ALIGN_RIGHT_MID, -8, 0);

  lv_obj_t *r1 = setRow(scrSet, 248, tr("横屏", "Landscape"));
  swLand = lv_switch_create(r1);
  lv_obj_align(swLand, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_add_event_cb(swLand, evLand, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *r2 = setRow(scrSet, 314, tr("语言", "Language"));
  lv_obj_add_event_cb(r2, evLang, LV_EVENT_CLICKED, nullptr);
  labLang = lv_label_create(r2);
  lv_label_set_text(labLang, "中文");
  lv_obj_set_style_text_font(labLang, cn(), 0);
  lv_obj_align(labLang, LV_ALIGN_RIGHT_MID, -12, 0);

  labTouch = lv_label_create(scrSet);
  lv_label_set_text(labTouch, "");
  lv_obj_set_style_text_color(labTouch, lv_color_hex(0x888888), 0);
  lv_obj_align(labTouch, LV_ALIGN_TOP_MID, 0, 382);
  lv_obj_set_style_pad_bottom(scrSet, 28, 0);
}

static void buildCal() {
  scrCal = lv_obj_create(nullptr);
  bw::styleScreenDark(scrCal);
  lv_obj_clear_flag(scrCal, LV_OBJ_FLAG_SCROLLABLE);
  makeTitle(scrCal, tr("触摸校准", "Touch cal"), 0xFFFFFF);

  labCal = lv_label_create(scrCal);
  lv_label_set_text(labCal, tr("点左上角绿点", "tap green"));
  lv_obj_set_style_text_font(labCal, cn(), 0);
  lv_obj_align(labCal, LV_ALIGN_TOP_MID, 0, 48);

  UiCalGeom box{};
  uiCalGeom(&box);

  btnCalA = lv_btn_create(scrCal);
  lv_obj_set_pos(btnCalA, box.gx, box.gy);
  lv_obj_set_size(btnCalA, box.gw, box.gh);
  lv_obj_set_style_radius(btnCalA, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btnCalA, lv_color_hex(0x00C853), 0);
  lv_obj_add_event_cb(btnCalA, evCalA, LV_EVENT_CLICKED, nullptr);

  btnCalB = lv_btn_create(scrCal);
  lv_obj_set_pos(btnCalB, box.bx, box.by);
  lv_obj_set_size(btnCalB, box.bw, box.bh);
  lv_obj_set_style_radius(btnCalB, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btnCalB, lv_color_hex(0x00B0FF), 0);
  lv_obj_add_event_cb(btnCalB, evCalB, LV_EVENT_CLICKED, nullptr);

  btnCalDone = lv_btn_create(scrCal);
  lv_obj_set_size(btnCalDone, 200, 40);
  lv_obj_align(btnCalDone, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(btnCalDone, lv_color_hex(0x00C853), 0);
  lv_obj_add_event_cb(btnCalDone, evCalDone, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ld = lv_label_create(btnCalDone);
  lv_label_set_text(ld, tr("完成", "Done"));
  lv_obj_set_style_text_font(ld, cn(), 0);
  lv_obj_set_style_text_color(ld, lv_color_hex(0x000000), 0);
  lv_obj_center(ld);
}

static lv_obj_t *lteLine(lv_obj_t *parent, int16_t y, lv_obj_t **outLab) {
  lv_obj_t *lab = lv_label_create(parent);
  lv_label_set_text(lab, "--");
  lv_obj_set_style_text_font(lab, cn(), 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(0xDDDDDD), 0);
  lv_obj_set_width(lab, 216);
  lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
  lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, y);
  if (outLab) {
    *outLab = lab;
  }
  return lab;
}

static void buildLte() {
  scrLte = lv_obj_create(nullptr);
  bw::styleScreenDark(scrLte);
  lv_obj_clear_flag(scrLte, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *b = lv_btn_create(scrLte);
  lv_obj_align(b, LV_ALIGN_TOP_LEFT, 6, 6);
  bw::styleBackBtn(b);
  lv_obj_set_ext_click_area(b, 12);
  lv_obj_add_flag(b, LV_OBJ_FLAG_FLOATING);
  lv_obj_move_foreground(b);
  lv_obj_add_event_cb(b, evBackToSet, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *bl = lv_label_create(b);
  lv_label_set_text(bl, tr("返回", "Back"));
  styleLabelCn(bl, 0xFFFFFF);
  lv_obj_center(bl);
  lv_obj_clear_flag(bl, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *t = lv_label_create(scrLte);
  lv_label_set_text(t, tr("4G自测", "4G test"));
  styleLabelCn(t, 0xFFFFFF);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

  labLteStatus = lv_label_create(scrLte);
  lv_label_set_text(labLteStatus, tr("测试中", "testing"));
  lv_obj_set_style_text_font(labLteStatus, cn(), 0);
  lv_obj_set_style_text_color(labLteStatus, lv_color_hex(0xFFC107), 0);
  lv_obj_align(labLteStatus, LV_ALIGN_TOP_MID, 0, 52);

  lteLine(scrLte, 88, &labLteId);
  lteLine(scrLte, 120, &labLteSim);
  lteLine(scrLte, 152, &labLteCsq);
  lteLine(scrLte, 188, &labLteHint);
  lv_obj_set_style_text_color(labLteHint, lv_color_hex(0x888888), 0);

  lv_obj_t *retry = bw::makePillBtn(scrLte, 168, 40, 268, bw::kAccentBlue);
  lv_obj_align(retry, LV_ALIGN_TOP_MID, 0, 268);
  lv_obj_add_event_cb(retry, evLteRetry, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *rl = lv_label_create(retry);
  lv_label_set_text(rl, tr("再测", "Retry"));
  lv_obj_set_style_text_font(rl, cn(), 0);
  lv_obj_center(rl);
}

void bwUiStart() {
  buildHome();
  buildMenu();
  buildCtrl();
  buildSet();
  buildCal();
  buildLte();
  lv_scr_load(scrHome);
  bwUiPatchClock();
}

void bwUiFlush() {
  lv_refr_now(nullptr);
}

void bwUiTick() {
  lvPortTick();
  static uint32_t lastClockMs = 0;
  if (millis() - lastClockMs >= 1000UL) {
    lastClockMs = millis();
    bwUiPatchClock();
  }
  if (recOn && listenMode) {
    const lv_opa_t pulse = static_cast<lv_opa_t>(40 + ((millis() / 120) % 4) * 40);
    for (int i = 0; i < 3; ++i) {
      if (objGlow[i]) {
        lv_obj_set_style_border_opa(objGlow[i], pulse, 0);
      }
    }
  }
  if (imgLumi && lv_scr_act() == scrHome) {
    static uint32_t lastFrameMs = 0;
    if (millis() - lastFrameMs >= LUMI_FRAME_MS) {
      lastFrameMs = millis();
      if (listenMode && LUMI_LISTEN_N > 0) {
        listenFrame = static_cast<uint8_t>((listenFrame + 1) % LUMI_LISTEN_N);
        lumiSetFrame(static_cast<uint8_t>(LUMI_LISTEN_OFF + listenFrame));
      } else if (LUMI_IDLE_N > 0) {
        idleFrame = static_cast<uint8_t>((idleFrame + 1) % LUMI_IDLE_N);
        lumiSetFrame(idleFrame);
      }
    }
  }
}

void bwUiPatchRecLevel(uint8_t level) {
  if (!recOn || !objVuBg || !objVuFill) {
    return;
  }
  if (level > 100) {
    level = 100;
  }
  lv_obj_clear_flag(objVuBg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(objVuFill, static_cast<lv_coord_t>(8 + (170 * level) / 100));
}

void bwUiSetRecord(bool on, uint8_t level, const char *status) {
  const bool themeChange = (on != recOn);
  recOn = on;
  if (themeChange) {
    applyHomeTheme(on);
  }
  if (labHint) {
    if (on) {
      lv_obj_add_flag(labHint, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(labHint, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (objVuBg) {
    if (on) {
      lv_obj_clear_flag(objVuBg, LV_OBJ_FLAG_HIDDEN);
      if (level > 100) {
        level = 100;
      }
      const lv_coord_t w = static_cast<lv_coord_t>(8 + (170 * level) / 100);
      lv_obj_set_width(objVuFill, w);
      lv_obj_set_style_bg_color(objVuFill, lv_color_hex(bw::kWifiGreen), 0);
    } else {
      lv_obj_add_flag(objVuBg, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_width(objVuFill, 8);
    }
  }
  if (btnTalk && labTalk) {
    if (on) {
      lv_obj_set_style_bg_color(btnTalk, lv_color_hex(bw::kPillDark), 0);
      lv_label_set_text(labTalk, tr("正在聆听...", "Listening..."));
      lv_obj_set_style_text_color(labTalk, lv_color_hex(0xFFFFFF), 0);
      lv_obj_move_foreground(btnTalk);
    } else {
      lv_obj_set_style_bg_color(btnTalk, lv_color_hex(bw::kPillCoral), 0);
      lv_label_set_text(labTalk, tr("开始对话", "Talk"));
      lv_obj_set_style_text_color(labTalk, lv_color_hex(bw::kInk), 0);
    }
  }
  if (labSub) {
    if (on && status && status[0]) {
      lv_label_set_text(labSub, status);
    } else if (!on) {
      lv_obj_set_style_text_color(labSub, lv_color_hex(bw::kMuted), 0);
    }
  }
}

void bwUiPatchClock() {
  if (!labTime) {
    return;
  }
  char buf[8];
  uiFillClock(buf, sizeof(buf));
  if (strcmp(lv_label_get_text(labTime), buf) != 0) {
    lv_label_set_text(labTime, buf);
  }
}

void bwUiSetSubtitle(const char *subtitle) {
  if (labSub) {
    lv_label_set_text(labSub, subtitle ? subtitle : "");
  }
}

void bwUiSetPair(const char *line) {
  if (labPair) {
    lv_label_set_text(labPair, line && line[0] ? line : "PAIR ----");
  }
}

bool bwUiHitTalk(int16_t x, int16_t y) {
  // Pill btn: 200×36, ALIGN_BOTTOM_MID, y offset -10 on 240×320
  return x >= 16 && x <= 224 && y >= 262 && y <= 318;
}

void bwUiShowHome(const char *subtitle, uint32_t faceHex) {
  if (labSub) {
    if (subtitle && subtitle[0]) {
      lv_label_set_text(labSub, subtitle);
    } else {
      lv_label_set_text(labSub, tr("待机", "idle"));
    }
  }
  (void)faceHex;
  loadScr(scrHome, false);
}

void bwUiShowMenu() { loadScr(scrMenu, true); }

void bwUiShowControl() {
  bwUiPatchControl(prefs().volume, prefs().brightness, prefs().dnd);
  loadScr(scrCtrl, true);
}

void bwUiShowSettings() {
  bwUiPatchSettings(prefs().landscape ? tr("横屏", "Land") : tr("竖屏", "Port"), prefsLangName(), touchModeName());
  loadScr(scrSet, true);
}

void bwUiShowCal() { loadScr(scrCal, true); }

void bwUiShowLte() { loadScr(scrLte, true); }

void bwUiPatchLte(const char *status, const char *id, const char *sim, const char *csq, const char *hint,
                  uint32_t statusColor) {
  if (labLteStatus) {
    lv_label_set_text(labLteStatus, status ? status : "--");
    lv_obj_set_style_text_color(labLteStatus, lv_color_hex(statusColor), 0);
  }
  if (labLteId) {
    lv_label_set_text(labLteId, id ? id : "--");
  }
  if (labLteSim) {
    lv_label_set_text(labLteSim, sim ? sim : "--");
  }
  if (labLteCsq) {
    lv_label_set_text(labLteCsq, csq ? csq : "--");
  }
  if (labLteHint) {
    lv_label_set_text(labLteHint, hint ? hint : "");
  }
}

void bwUiPatchControl(uint8_t volume, uint8_t brightness, bool dnd) {
  if (swDnd) {
    if (dnd) {
      lv_obj_add_state(swDnd, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(swDnd, LV_STATE_CHECKED);
    }
  }
  if (slBright) {
    lv_slider_set_value(slBright, brightness, LV_ANIM_OFF);
  }
  if (slVol) {
    lv_slider_set_value(slVol, volume, LV_ANIM_OFF);
  }
  const char *bn[] = {"低", "中", "高"};
  const char *vn[] = {"静音", "低", "中", "高"};
  if (labBright) {
    lv_label_set_text(labBright, bn[brightness % 3]);
  }
  if (labVol) {
    lv_label_set_text(labVol, vn[volume % 4]);
  }
}

void bwUiPatchSettings(const char *orient, const char *lang, const char *touchMode) {
  (void)orient;
  if (swLand) {
    if (prefs().landscape) {
      lv_obj_add_state(swLand, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(swLand, LV_STATE_CHECKED);
    }
  }
  if (labLang) {
    lv_label_set_text(labLang, langIsEn() ? "English" : "中文");
  }
  if (labTouch && touchMode) {
    char buf[40];
    snprintf(buf, sizeof(buf), "TP %s", touchMode);
    lv_label_set_text(labTouch, buf);
  }
  (void)lang;
}

void bwUiPatchCal(uint8_t step, int16_t x, int16_t y, bool down, const char *mapName) {
  (void)x;
  (void)y;
  (void)down;
  if (!labCal) {
    return;
  }
  if (step == 0) {
    lv_label_set_text(labCal, tr("点左上角绿点", "tap green"));
  } else if (step == 1) {
    lv_label_set_text(labCal, tr("点右下角蓝点", "tap blue"));
  } else {
    char buf[48];
    snprintf(buf, sizeof(buf), "%s  %s", tr("按住绿点或点完成", "hold green / done"), mapName ? mapName : "");
    lv_label_set_text(labCal, buf);
  }
}
