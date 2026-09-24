/* Teste minimo de display+touch — copia do projeto de referencia que funciona
 * na Waveshare ESP32-S3-Touch-AMOLED-1.64 (jaapp/waveshare-amoled164-lvgl9).
 * Se AQUI a tela ficar boa, o problema esta na config do app principal.
 */
#include <Arduino.h>
#include <lvgl.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

Arduino_DataBus *bus = new Arduino_ESP32QSPI(9, 10, 11, 12, 13, 14);
Arduino_GFX *g = new Arduino_CO5300(bus, 21, 0, 280, 456, 20, 0, 180, 24);
Arduino_GFX *gfx = g;

#define FT_ADDR 0x38
static uint16_t tX, tY;
static bool tDown;
static void ft_poll() {
  uint8_t b[5] = {0};
  Wire.beginTransmission(FT_ADDR);
  Wire.write((uint8_t)0x02);
  if (Wire.endTransmission(false) != 0) { tDown = false; return; }
  if (Wire.requestFrom(FT_ADDR, 5) != 5) { tDown = false; return; }
  for (int i = 0; i < 5; i++) b[i] = Wire.read();
  if (b[0] & 0x0F) {
    tX = ((b[1] & 0x0F) << 8) | b[2];
    tY = ((b[3] & 0x0F) << 8) | b[4];
    tDown = true;
  } else tDown = false;
}
static void touch_cb(lv_indev_t *, lv_indev_data_t *d) {
  ft_poll();
  if (tDown) { d->state = LV_INDEV_STATE_PRESSED; d->point.x = tX; d->point.y = tY; }
  else d->state = LV_INDEV_STATE_RELEASED;
}

uint32_t screenWidth, screenHeight, bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;
static lv_obj_t *circle, *hline, *vline, *lbl;

static uint32_t millis_cb(void) { return millis(); }

void my_disp_rounder_cb(lv_event_t *e) {
  lv_area_t *area = (lv_area_t *)lv_event_get_param(e);
  area->x1 = 0;
  area->x2 = screenWidth - 1;
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(px_map, w * h);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("TESTE DISPLAY — inicio");

  if (!gfx->begin()) Serial.println("gfx begin FALHOU");
  gfx->fillScreen(RGB565_BLACK);
  Serial.printf("gfx %ux%u\n", gfx->width(), gfx->height());

  lv_init();
  lv_tick_set_cb(millis_cb);

  screenWidth = gfx->width();
  screenHeight = gfx->height();
  bufSize = screenWidth * 40;
  disp_draw_buf = (lv_color_t *)heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!disp_draw_buf)
    disp_draw_buf = (lv_color_t *)heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, bufSize * 2, MALLOC_CAP_8BIT);
  Serial.printf("buf %u px @%p\n", bufSize, disp_draw_buf);

  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(disp, my_disp_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);

  Wire.begin(47, 48);
  Wire.setClock(300000);
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);

  // UI de teste: relogio count-up + marcadores de touch
  lbl = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(lbl, "TESTE DISPLAY");
  lv_obj_center(lbl);

  circle = lv_obj_create(lv_screen_active());
  lv_obj_set_size(circle, 24, 24);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(circle, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_border_opa(circle, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(circle, LV_OBJ_FLAG_HIDDEN);
  hline = lv_obj_create(lv_screen_active());
  lv_obj_set_size(hline, screenWidth, 2);
  lv_obj_set_style_bg_color(hline, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_border_opa(hline, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(hline, LV_OBJ_FLAG_HIDDEN);
  vline = lv_obj_create(lv_screen_active());
  lv_obj_set_size(vline, 2, screenHeight);
  lv_obj_set_style_bg_color(vline, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_border_opa(vline, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(vline, LV_OBJ_FLAG_HIDDEN);

  Serial.println("TESTE DISPLAY — pronto");
}

void loop() {
  static uint32_t t0 = 0, n = 0;
  if (millis() - t0 > 1000) {
    t0 = millis();
    char b[32];
    snprintf(b, sizeof(b), "TESTE DISPLAY %lus", (unsigned long)(++n));
    lv_label_set_text(lbl, b);
    Serial.printf("[T] %lus\n", (unsigned long)n);
  }
  if (tDown) {
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(circle, tX - 12, tY - 12);
    lv_obj_clear_flag(hline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(hline, tY - 1);
    lv_obj_clear_flag(vline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(vline, tX - 1);
  } else {
    lv_obj_add_flag(circle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(hline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(vline, LV_OBJ_FLAG_HIDDEN);
  }
  uint32_t d = lv_timer_handler();
  delay(d < 5 ? 5 : (d > 20 ? 20 : d));
}
