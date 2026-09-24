/* lv_conf.h minimo — LVGL 9.6 (opcoes nao listadas usam defaults internos) */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Cor: RGB565 (o swap de bytes e feito no flush com lv_draw_sw_rgb565_swap) */
#define LV_COLOR_DEPTH 16

/* Pool de memoria do LVGL (heap interno do ESP32-S3) */
#define LV_MEM_SIZE (96 * 1024)

/* Fontes usadas pela UI (Montserrat built-in, apenas ASCII — usar texto sem acento) */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1

/* Clock via millis() */
#define LV_TICK_CUSTOM 0

#endif
