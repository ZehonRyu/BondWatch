#include "lv_port.h"
#include "touch.h"
#include "pins.h"

#include <lvgl.h>
#include <Arduino.h>
#include <esp_heap_caps.h>

static Adafruit_ST7789 *disp = nullptr;
static lv_disp_draw_buf_t drawBuf;
static lv_color_t *buf1 = nullptr;
static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;

static const uint16_t BUF_LINES = 40;

static void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  if (!disp) {
    lv_disp_flush_ready(drv);
    return;
  }
  const int16_t w = static_cast<int16_t>(area->x2 - area->x1 + 1);
  const int16_t h = static_cast<int16_t>(area->y2 - area->y1 + 1);
  disp->startWrite();
  disp->setAddrWindow(area->x1, area->y1, w, h);
  disp->writePixels(reinterpret_cast<uint16_t *>(color_p), static_cast<uint32_t>(w) * static_cast<uint32_t>(h));
  disp->endWrite();
  lv_disp_flush_ready(drv);
}

static void touchCb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;
  int16_t x = 0;
  int16_t y = 0;
  bool down = false;
  touchPointer(&x, &y, &down);
  if (down) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void lvPortBegin(Adafruit_ST7789 *tft) {
  disp = tft;
  lv_init();
  const size_t n = static_cast<size_t>(240) * BUF_LINES;
  buf1 = static_cast<lv_color_t *>(heap_caps_malloc(n * sizeof(lv_color_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
  if (!buf1) {
    buf1 = static_cast<lv_color_t *>(malloc(n * sizeof(lv_color_t)));
  }
  lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, n);
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = 240;
  dispDrv.ver_res = 320;
  dispDrv.flush_cb = flushCb;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_t *d = lv_disp_drv_register(&dispDrv);
  lv_theme_t *th = lv_theme_default_init(d, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_CYAN), true,
                                         LV_FONT_DEFAULT);
  lv_disp_set_theme(d, th);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchCb;
  lv_indev_drv_register(&indevDrv);
}

void lvPortTick() {
  lv_timer_handler();
}
