/*
 * Estação Meteorológica Mini
 * Waveshare ESP32-S3-Touch-AMOLED-1.64 (CO5300 QSPI 280x456 + FT3168 touch + QMI8658 IMU)
 * Sensor externo: Adafruit BMP581 (I2C 0x47 no barramento GPIO47/48)
 *
 * - Relógio NTP + data (fuso via geo-IP, ajustável pela web)
 * - Previsão/tempo atual via Open-Meteo (localização via geo-IP ou manual na web)
 * - BMP581: temperatura/pressão locais; QMI8658: giro/acel
 * - Auto-rotação (retrato/paisagem) pelo acelerômetro
 * - Touch: navegação por abas
 * - Web server: http://estacao.local ou IP — monitor + configuração (WiFi, local, fuso)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>
#include "Adafruit_BMP5xx.h"
#include "icon_sun.h"
#include "icon_cloud.h"
#include "icon_suncloud.h"
#include "icon_rain.h"
#include "icon_storm.h"
#include "icon_sunrain.h"
#include "icon_snow.h"
#include "icon_fog.h"

// ============================ WiFi (credenciais iniciais) ====================
#define WIFI_SSID_DEFAULT "Desktop_F8423541"
#define WIFI_PASS_DEFAULT "Inicial123"

// ============================ Pinos / hardware ===============================
#define TOUCH_SDA 47
#define TOUCH_SCL 48
#define I2C_HZ 300000

#define QMI_ADDR 0x6B
#define FT_ADDR 0x38
#define BMP_ADDR BMP5XX_ALTERNATIVE_ADDRESS // 0x47

Arduino_DataBus *bus = new Arduino_ESP32QSPI(9, 10, 11, 12, 13, 14);
Arduino_GFX *gfx = new Arduino_CO5300(bus, 21 /*RST*/, 0 /*rot NATIVO — 180° fica por conta do LVGL*/, 280, 456,
                                      20, 0, 180, 24);

// ============================ Config persistente =============================
Preferences prefs;
struct Cfg {
  char ssid[33];
  char pass[65];
  float lat;
  float lon;
  char city[40];
  char tz[64];
} cfg;

void cfgLoad() {
  memset(&cfg, 0, sizeof(cfg));
  prefs.begin("estacao", true);
  String s;
  s = prefs.getString("ssid", WIFI_SSID_DEFAULT);
  strlcpy(cfg.ssid, s.c_str(), sizeof(cfg.ssid));
  s = prefs.getString("pass", WIFI_PASS_DEFAULT);
  strlcpy(cfg.pass, s.c_str(), sizeof(cfg.pass));
  cfg.lat = prefs.getFloat("lat", 0);
  cfg.lon = prefs.getFloat("lon", 0);
  s = prefs.getString("city", "");
  strlcpy(cfg.city, s.c_str(), sizeof(cfg.city));
  s = prefs.getString("tz", "America/Sao_Paulo");
  strlcpy(cfg.tz, s.c_str(), sizeof(cfg.tz));
  prefs.end();
}
void cfgSave() {
  prefs.begin("estacao", false);
  prefs.putString("ssid", cfg.ssid);      // setString grava '\0' final
  prefs.putString("pass", cfg.pass);
  prefs.putFloat("lat", cfg.lat);
  prefs.putFloat("lon", cfg.lon);
  prefs.putString("city", cfg.city);
  prefs.putString("tz", cfg.tz);
  prefs.end();
}

// ============================ Estado / sensores ==============================
enum SensorSt : uint8_t { ST_WAIT = 0, ST_OK, ST_FAIL };
struct State {
  SensorSt bmp = ST_WAIT, qmi = ST_WAIT;
  float bmpTemp = 0, bmpPress = 0;        // °C, hPa
  float gyro[3] = {0, 0, 0};              // °/s
  float acc[3] = {0, 0, 0};               // g
  float outdoorTemp = 0, tmin = 0, tmax = 0;
  int wcode = -1;
  bool wxValid = false;
  char wxDesc[40] = "";
  int rainProb = -1;       // prob. chuva 1h (%)
  int rainInH = -1;        // chuva comecando em X horas (-1 = sem previsao)
  float rainMm = 0;
  float humidity = -1;     // umidade relativa % (Open-Meteo)
  float pressMsl = 0;      // pressao nivel do mar hPa (Open-Meteo)
  float altitude = 0;      // m, barometrica (BMP581 vs MSL)
  float lat = 0, lon = 0;
  char locName[48] = "";
  char tzName[64] = "America/Sao_Paulo";
  int rot = 0;                            // 0/1/2/3
  bool apMode = false;
  uint32_t wxAgeMin = 999;
} st;

time_t lastNtpSync = 0;

// ============================ QMI8658 (IMU onboard) ==========================
bool qmiInit() {
  uint8_t id = 0;
  Wire.beginTransmission(QMI_ADDR);
  Wire.write((uint8_t)0x00); // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(QMI_ADDR, 1) != 1) return false;
  id = Wire.read();
  Serial.printf("[QMI] WHO_AM_I=0x%02X\n", id);
  if (id != 0x05) return false;
  // reset
  Wire.beginTransmission(QMI_ADDR); Wire.write(0x60); Wire.write(0xB0); Wire.endTransmission();
  delay(15);
  // CTRL1=0x60: ADDR_AI (auto-incremento I2C) + SPI 4-wire
  Wire.beginTransmission(QMI_ADDR); Wire.write(0x02); Wire.write(0x60); Wire.endTransmission();
  // acc +/-8g 1008Hz, gyro 1008Hz
  Wire.beginTransmission(QMI_ADDR); Wire.write(0x32); Wire.write(0x24); Wire.endTransmission();
  Wire.beginTransmission(QMI_ADDR); Wire.write(0x33); Wire.write(0x23); Wire.endTransmission();
  // habilita acc+gyro
  Wire.beginTransmission(QMI_ADDR); Wire.write(0x08); Wire.write(0x03); Wire.endTransmission();
  delay(2);
  return true;
}

bool qmiRead() {
  // Datasheet: TEMP_L=0x33, AX=0x35, AY=0x37, AZ=0x39, GX=0x3B, GY=0x3D, GZ=0x3F
  Wire.beginTransmission(QMI_ADDR);
  Wire.write((uint8_t)0x33);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(QMI_ADDR, 14) != 14) return false;
  uint8_t b[14];
  for (int i = 0; i < 14; i++) b[i] = Wire.read();
  int16_t raw;
  raw = (int16_t)((b[2] << 8) | b[3]);  st.acc[0] = raw / 4096.0f; // ±8g
  raw = (int16_t)((b[4] << 8) | b[5]);  st.acc[1] = raw / 4096.0f;
  raw = (int16_t)((b[6] << 8) | b[7]);  st.acc[2] = raw / 4096.0f;
  raw = (int16_t)((b[8] << 8) | b[9]);  st.gyro[0] = raw / 128.0f; // ±250dps
  raw = (int16_t)((b[10] << 8) | b[11]); st.gyro[1] = raw / 128.0f;
  raw = (int16_t)((b[12] << 8) | b[13]); st.gyro[2] = raw / 128.0f;
  return true;
}

// ============================ BMP581 (I2C externo) ===========================
Adafruit_BMP5xx bmp;
bool bmpRead() {
  if (!bmp.performReading()) return false;
  st.bmpTemp = bmp.temperature;
  st.bmpPress = bmp.pressure; // a lib ja retorna hPa
  return true;
}

// ============================ LVGL: display ==================================
static lv_color_t *dbuf;
static lv_display_t *disp;
static uint32_t scrW, scrH, bufPx;

static uint32_t millis_cb() { return millis(); }

static void disp_flush(lv_display_t *d, const lv_area_t *area, uint8_t *px) {
  uint32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  // LVGL 9 entrega RGB565 na ordem que o Arduino_GFX espera — NAO fazer swap
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px, w, h);
  lv_display_flush_ready(d);
}
static void disp_rounder(lv_event_t *e) {
  lv_area_t *a = (lv_area_t *)lv_event_get_param(e);
  a->x1 = 0; a->x2 = (int32_t)scrW - 1;
}

// ============================ LVGL: touch FT3168 =============================
static int16_t tX, tY;
static bool tDown;
static void ft_poll() {
  uint8_t b[5] = {0};
  Wire.beginTransmission(FT_ADDR);
  Wire.write((uint8_t)0x02);
  if (Wire.endTransmission(false) != 0) { tDown = false; return; }
  if (Wire.requestFrom(FT_ADDR, 5) != 5) { tDown = false; return; }
  for (int i = 0; i < 5; i++) b[i] = Wire.read();
  if (b[0] & 0x0F) {
    uint16_t x = ((b[1] & 0x0F) << 8) | b[2];
    uint16_t y = ((b[3] & 0x0F) << 8) | b[4];
    tX = (int16_t)x; tY = (int16_t)y; tDown = true;
  } else tDown = false;
}
static uint32_t tLastTouchLog = 0;
static bool lastDown = false;
static void touch_cb(lv_indev_t *, lv_indev_data_t *d) {
  ft_poll();
  if (tDown && !lastDown) Serial.printf("[TOUCH] down raw=%d,%d\n", tX, tY);
  if (!tDown && lastDown) Serial.println("[TOUCH] up");
  if (tDown && millis() - tLastTouchLog > 500) {
    tLastTouchLog = millis();
    Serial.printf("[TOUCH] held raw=%d,%d\n", tX, tY);
  }
  lastDown = tDown;
  if (tDown) {
    d->state = LV_INDEV_STATE_PRESSED;
    // coordenadas CRUAS: o LVGL 9 aplica a rotacao 180 do display no indev sozinho;
    // inverter aqui tambem = dupla inversao = toque espelhado
    d->point.x = tX;
    d->point.y = tY;
  }
  else d->state = LV_INDEV_STATE_RELEASED;
}

// ============================ UI =============================================
static lv_obj_t *tabHome, *tabSensors, *tabCfg;
static lv_obj_t *navRow, *btnHome, *btnSens, *btnCfg;
static lv_obj_t *lblClock, *lblDate, *lblWxTemp, *lblWxDesc, *lblWxMini;
static lv_obj_t *lblBmpT, *lblBmpP, *lblWxOut, *lblAlt, *lblHum;
static lv_obj_t *imgWx;        // emoji do clima
static lv_obj_t *lblRainAlert; // banner "leva guarda-chuva"
static lv_obj_t *cRain;        // card do alerta
static lv_obj_t *lblGyro, *lblAcc, *lblStBmp, *lblStQmi, *lblStWx, *lblStNet, *lblRot;
static lv_obj_t *lblCfgInfo;
static lv_obj_t *cClock, *cWx, *cS, *cSt, *cG;
static lv_obj_t *kb = nullptr; // (removido — config via web)
static lv_style_t stCard, stTitle;

static const char *wcodeDesc(int c) {
  switch (c) {
    case 0: return "Ceu limpo";
    case 1: case 2: return "Parc. nublado";
    case 3: return "Nublado";
    case 45: case 48: return "Nevoeiro";
    case 51: case 53: case 55: return "Garoa";
    case 61: case 63: case 65: return "Chuva";
    case 66: case 67: return "Chuva congel.";
    case 71: case 73: case 75: return "Neve";
    case 80: case 81: case 82: return "Pancadas";
    case 95: return "Tempestade";
    case 96: case 99: return "Tmd. c/ granizo";
    default: return "-";
  }
}

static const lv_image_dsc_t *wcodeIcon(int c) {
  if (c == 0 || c == 1) return &icon_sun;
  if (c == 2) return &icon_suncloud;
  if (c == 3) return &icon_cloud;
  if (c == 45 || c == 48) return &icon_fog;
  if (c >= 51 && c <= 67) return &icon_rain;
  if (c >= 71 && c <= 77) return &icon_snow;
  if (c >= 80 && c <= 82) return &icon_sunrain;
  if (c >= 95) return &icon_storm;
  return &icon_cloud;
}

static void navTo(lv_obj_t *tab, lv_obj_t *btn) {
  lv_obj_t *tabs[] = {tabHome, tabSensors, tabCfg};
  lv_obj_t *btns[] = {btnHome, btnSens, btnCfg};
  for (int i = 0; i < 3; i++) {
    lv_obj_add_flag(tabs[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(btns[i], lv_color_hex(0x1a1a26), 0);
  }
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x2f6fed), 0);
  lv_obj_scroll_to(tab, 0, 0, LV_ANIM_OFF);
}
static void cb_nav_home(lv_event_t *) { navTo(tabHome, btnHome); }
static void cb_nav_sens(lv_event_t *) { navTo(tabSensors, btnSens); }
static void cb_nav_cfg(lv_event_t *) { navTo(tabCfg, btnCfg); }

static void cb_reboot(lv_event_t *) { delay(150); ESP.restart(); }

static lv_obj_t *makeCard(lv_obj_t *parent) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_add_style(c, &stCard, 0);
  return c;
}
static lv_obj_t *mkLabel(lv_obj_t *parent, const lv_font_t *f, uint32_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, "-");
  return l;
}

static void uiInit() {
  lv_style_init(&stCard);
  lv_style_set_bg_color(&stCard, lv_color_hex(0x14141f));
  lv_style_set_border_color(&stCard, lv_color_hex(0x2a2a3a));
  lv_style_set_border_width(&stCard, 1);
  lv_style_set_radius(&stCard, 10);
  lv_style_set_pad_all(&stCard, 8);

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a0a12), 0);

  // ---------- Home ----------
  tabHome = lv_obj_create(scr);
  lv_obj_set_style_bg_opa(tabHome, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tabHome, 0, 0);
  lv_obj_set_size(tabHome, scrW, scrH - 60);
  lv_obj_clear_flag(tabHome, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(tabHome, 6, 0);

  cClock = makeCard(tabHome);
  lblClock = mkLabel(cClock, &lv_font_montserrat_48, 0xffffff);
  lblDate = mkLabel(cClock, &lv_font_montserrat_14, 0x8fa3c8);

  lv_obj_t *cWx2 = makeCard(tabHome);
  imgWx = lv_image_create(cWx2);
  lv_image_set_src(imgWx, &icon_cloud);
  lblWxTemp = mkLabel(cWx2, &lv_font_montserrat_28, 0xffc857);
  lblWxDesc = mkLabel(cWx2, &lv_font_montserrat_14, 0xd0d8e8);
  lblWxMini = mkLabel(cWx2, &lv_font_montserrat_12, 0x8fa3c8);
  cWx = cWx2;

  // banner de alerta de chuva
  cRain = makeCard(tabHome);
  lv_obj_set_style_bg_color(cRain, lv_color_hex(0x3a2a10), 0);
  lv_obj_set_style_border_color(cRain, lv_color_hex(0x8a6a1a), 0);
  lblRainAlert = mkLabel(cRain, &lv_font_montserrat_14, 0xffd166);
  lv_label_set_long_mode(lblRainAlert, LV_LABEL_LONG_WRAP);
  lv_obj_add_flag(cRain, LV_OBJ_FLAG_HIDDEN);

  cS = makeCard(tabHome);
  lblBmpT = mkLabel(cS, &lv_font_montserrat_28, 0x6fd3ff);
  lblBmpP = mkLabel(cS, &lv_font_montserrat_28, 0x9d8cff);
  lblAlt = mkLabel(cS, &lv_font_montserrat_20, 0x7ddf87);
  lblHum = mkLabel(cS, &lv_font_montserrat_20, 0xffd166);
  lblWxOut = mkLabel(cS, &lv_font_montserrat_12, 0x8fa3c8);

  cSt = makeCard(tabHome);
  lblStBmp = mkLabel(cSt, &lv_font_montserrat_12, 0x666677);
  lblStQmi = mkLabel(cSt, &lv_font_montserrat_12, 0x666677);
  lblStWx = mkLabel(cSt, &lv_font_montserrat_12, 0x666677);
  lblStNet = mkLabel(cSt, &lv_font_montserrat_12, 0x666677);

  // ---------- Sensores ----------
  tabSensors = lv_obj_create(scr);
  lv_obj_set_style_bg_opa(tabSensors, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tabSensors, 0, 0);
  lv_obj_set_size(tabSensors, scrW, scrH - 60);
  lv_obj_set_flex_flow(tabSensors, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(tabSensors, 6, 0);
  lv_obj_set_style_pad_row(tabSensors, 8, 0);
  lv_obj_add_flag(tabSensors, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_scroll_dir(tabSensors, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(tabSensors, LV_SCROLLBAR_MODE_AUTO);

  cG = makeCard(tabSensors);
  lv_obj_set_width(cG, lv_pct(100));
  lv_obj_set_height(cG, LV_SIZE_CONTENT);
  lblGyro = mkLabel(cG, &lv_font_montserrat_14, 0xd0d8e8);
  lv_obj_align(lblGyro, LV_ALIGN_TOP_LEFT, 4, 2);
  lblAcc = mkLabel(cG, &lv_font_montserrat_14, 0xd0d8e8);
  lv_obj_align(lblAcc, LV_ALIGN_TOP_LEFT, 4, 24);
  lblRot = mkLabel(cG, &lv_font_montserrat_12, 0x8fa3c8);
  lv_obj_align(lblRot, LV_ALIGN_TOP_LEFT, 4, 46);

  // ---------- Config (só info — editar via web server) ----------
  tabCfg = lv_obj_create(scr);
  lv_obj_add_flag(tabCfg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(tabCfg, scrW, scrH - 60);
  lv_obj_set_flex_flow(tabCfg, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(tabCfg, 8, 0);
  lv_obj_set_style_pad_row(tabCfg, 8, 0);

  lv_obj_t *cInfo = makeCard(tabCfg);
  lv_obj_set_width(cInfo, lv_pct(100));
  lv_obj_set_height(cInfo, LV_SIZE_CONTENT);
  lblCfgInfo = mkLabel(cInfo, &lv_font_montserrat_14, 0xd0d8e8);
  lv_label_set_long_mode(lblCfgInfo, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lblCfgInfo, lv_pct(100));

  lv_obj_t *cReb = makeCard(tabCfg);
  lv_obj_set_width(cReb, lv_pct(100));
  lv_obj_set_height(cReb, LV_SIZE_CONTENT);
  lv_obj_t *btnReb = lv_btn_create(cReb);
  lv_obj_set_size(btnReb, 140, 46);
  lv_obj_add_event_cb(btnReb, cb_reboot, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *bl = lv_label_create(btnReb);
  lv_label_set_text(bl, "Reiniciar");
  lv_obj_center(bl);

  // ---------- Barra de navegação ----------
  navRow = lv_obj_create(scr);
  lv_obj_set_size(navRow, scrW, 60);
  lv_obj_set_style_bg_color(navRow, lv_color_hex(0x10101a), 0);
  lv_obj_set_style_border_width(navRow, 0, 0);
  lv_obj_clear_flag(navRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(navRow, 6, 0);
  lv_obj_set_style_pad_column(navRow, 4, 0);
  lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  auto mkNav = [&](const char *txt, lv_event_cb_t cb) {
    lv_obj_t *b = lv_btn_create(navRow);
    lv_obj_set_size(b, (scrW - 26) / 3, 46);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1a1a26), 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
    return b;
  };
  lv_obj_move_foreground(navRow);
  btnHome = mkNav("Clima", cb_nav_home);
  btnSens = mkNav("Sensores", cb_nav_sens);
  btnCfg = mkNav("Config", cb_nav_cfg);
  lv_obj_set_style_bg_color(btnHome, lv_color_hex(0x2456c4), 0);
}

static void layoutUi() {
  bool h = scrW > scrH;
  uint32_t cw = scrW - 12, ch;
  if (!h) { // retrato 280x412 util
    auto place = [&](lv_obj_t *c, int y, int hh) { lv_obj_set_pos(c, 6, y); lv_obj_set_size(c, cw, hh); };
    int y = 6;
    place(cClock, y, 84); y += 84 + 6;      // relógio
    place(cS, y, 128); y += 128 + 6;        // SENSORES (principal) 2x2 grande
    place(cWx, y, 78); y += 78 + 6;         // previsao compacta
    place(cRain, y, 44); y += 44 + 6;       // alerta chuva
    place(cSt, y, (int)scrH - 60 - 6 - y); // status
    lv_obj_align(lblClock, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_align(lblDate, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_align(imgWx, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_align(lblWxTemp, LV_ALIGN_TOP_RIGHT, -8, 6);
    lv_obj_align(lblWxDesc, LV_ALIGN_BOTTOM_RIGHT, -8, -26);
    lv_obj_align(lblWxMini, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    lv_obj_align(lblRainAlert, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(lblRainAlert, cw - 16);
    lv_obj_align(lblBmpT, LV_ALIGN_TOP_LEFT, 10, 4);
    lv_obj_align(lblBmpP, LV_ALIGN_TOP_RIGHT, -10, 4);
    lv_obj_align(lblAlt, LV_ALIGN_BOTTOM_LEFT, 10, -4);
    lv_obj_align(lblHum, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
    lv_obj_align(lblWxOut, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(lblStBmp, LV_ALIGN_TOP_LEFT, 6, 4);
    lv_obj_align(lblStQmi, LV_ALIGN_TOP_RIGHT, -6, 4);
    lv_obj_align(lblStWx, LV_ALIGN_BOTTOM_LEFT, 6, -4);
    lv_obj_align(lblStNet, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
  } else { // paisagem 456x(216-44)
    ch = (scrH - 60 - 6 * 2 - 6) / 2;
    lv_obj_set_size(tabHome, scrW, scrH - 60);
    lv_obj_set_pos(cClock, 6, 6);   lv_obj_set_size(cClock, cw / 4 - 3, ch);
    lv_obj_set_pos(cWx, 6 + cw / 4 + 3, 6); lv_obj_set_size(cWx, cw / 4 - 3, ch);
    lv_obj_set_pos(cRain, 6 + 2 * (cw / 4 + 3), 6); lv_obj_set_size(cRain, cw / 2 - 9, ch);
    lv_obj_set_pos(cS, 6, 6 + ch + 6); lv_obj_set_size(cS, cw / 2 - 3, ch);
    lv_obj_set_pos(cSt, 6 + cw / 2 + 3, 6 + ch + 6); lv_obj_set_size(cSt, cw / 2 - 9, ch);
    lv_obj_align(lblClock, LV_ALIGN_CENTER, 0, -14);
    lv_obj_align(lblDate, LV_ALIGN_CENTER, 0, 28);
    lv_obj_align(imgWx, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_align(lblWxTemp, LV_ALIGN_TOP_MID, 10, 2);
    lv_obj_align(lblWxDesc, LV_ALIGN_BOTTOM_MID, 10, -20);
    lv_obj_align(lblWxMini, LV_ALIGN_BOTTOM_MID, 10, 0);
    lv_obj_align(lblRainAlert, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(lblRainAlert, cw / 2 - 20);
    lv_obj_align(lblBmpT, LV_ALIGN_TOP_LEFT, 4, 2);
    lv_obj_align(lblBmpP, LV_ALIGN_TOP_LEFT, 4, 26);
    lv_obj_align(lblWxOut, LV_ALIGN_BOTTOM_LEFT, 4, 0);
    lv_obj_align(lblStBmp, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_obj_align(lblStQmi, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_align(lblStWx, LV_ALIGN_TOP_RIGHT, -2, 0);
    lv_obj_align(lblStNet, LV_ALIGN_BOTTOM_MID, 0, 0);
  }
  // sensores / config seguem a orientação
  lv_obj_set_size(tabSensors, scrW, scrH - 60);
  lv_obj_set_size(tabCfg, scrW, scrH - 60);
}

static void applyRotation(int rot) {
  // NUNCA chamar gfx->setRotation() em runtime: o CO5300 nao suporta
  // reprogramacao do MADCTL apos o init (corrompe a tela com listras).
  // Painel fixo em retrato (rot 0). IMU fica apenas como sensor.
  (void)rot;
}

// ============================ WiFi / hora / previsão =========================
WiFiMulti wifiMulti;
WebServer server(80);
DNSServer dns;

static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="pt-br"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Estacao Meteo</title><style>
*{box-sizing:border-box}body{font-family:system-ui,sans-serif;background:#0b0b13;color:#e8ecf4;margin:0;padding:16px;max-width:720px;margin:auto}
h1{font-size:1.25em;margin:.2em 0 .6em}.sub{color:#7c8bb0;font-size:.8em;margin-bottom:14px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.c{background:#14141f;border:1px solid #262640;border-radius:12px;padding:12px}
.k{color:#7c8bb0;font-size:.72em;text-transform:uppercase;letter-spacing:.05em}
.v{font-size:1.55em;font-weight:600;margin:4px 0 2px}.u{color:#7c8bb0;font-size:.8em}
.big{grid-column:1/-1;display:flex;align-items:center;gap:14px}
.big .v{font-size:2.6em;margin:0}
.wxicon{font-size:3em;line-height:1}
.ok{color:#57d98a}.bad{color:#ff7d7d}.warn{color:#ffd166}
.rain{grid-column:1/-1;background:#2a2210;border-color:#6a5514}
details{grid-column:1/-1;background:#14141f;border:1px solid #262640;border-radius:12px;padding:10px 12px}
summary{cursor:pointer;color:#9fb0d8;font-weight:600}
input{width:100%;padding:9px;margin:3px 0 10px;border-radius:8px;border:1px solid #262640;background:#0f0f18;color:#e8ecf4}
button{padding:11px 20px;border-radius:9px;border:0;background:#2f6fed;color:#fff;font-size:.95em;margin:4px 4px 0;cursor:pointer}
button:hover{background:#2456c4}#msg{color:#57d98a;margin-left:8px}
.row{display:flex;flex-wrap:wrap;gap:6px;color:#7c8bb0;font-size:.78em;margin-top:10px}
</style></head><body>
<h1>&#9729;&#65039; Estacao Meteorologica</h1>
<div class="sub" id="sub">carregando...</div>
<div class="grid" id="g"></div>
<details><summary>&#9881;&#65039; Configuracao</summary>
<div class="k">WiFi SSID</div><input id="ssid">
<div class="k">Senha WiFi</div><input id="pass" type="password">
<div style="display:flex;gap:10px"><div style="flex:1"><div class="k">Latitude</div><input id="lat"></div>
<div style="flex:1"><div class="k">Longitude</div><input id="lon"></div></div>
<div class="k">Fuso (TZ)</div><input id="tz">
<button onclick="save()">&#128190; Salvar</button>
<button onclick="location='/api/reboot'">&#128260; Reiniciar</button><span id="msg"></span>
</details>
<div class="row" id="foot"></div>
<script>
const WMO={0:['&#2600;&#65039;','Ceu limpo'],1:['&#127780;','Predom. sol'],2:['&#9925;','Parc. nublado'],3:['&#9729;&#65039;','Nublado'],45:['&#127787;','Nevoeiro'],48:['&#127787;','Nevoeiro'],51:['&#127783;','Garoa'],53:['&#127783;','Garoa'],55:['&#127783;','Garoa'],61:['&#127783;','Chuva fraca'],63:['&#127783;','Chuva'],65:['&#127783;','Chuva forte'],71:['&#10052;&#65039;','Neve'],80:['&#127783;','Pancadas'],81:['&#127783;','Pancadas'],82:['&#9889;','Pancadas fortes'],95:['&#9889;','Tempestade'],96:['&#9889;','Tempestade c/ granizo']};
function em(c){return (WMO[c]||['&#9925;','-'])[0]}
function ds(c){return (WMO[c]||['-'])[1]}
async function up(){
try{const r=await(await fetch('/api/status')).json();
const ok=v=>v?'<span class="ok">&#10003; OK</span>':'<span class="bad">&#10007; falha</span>';
document.getElementById('sub').innerHTML=`${r.loc} &middot; ${r.clock} &middot; IP ${r.ip}`;
let rain='';
if(r.rain_in_h===0)rain='<div class="c rain"><div class="k">&#9889; Alerta</div><div class="v warn">CHOVENDO AGORA</div><div class="u">'+r.rain_mm+' mm &mdash; leva guarda-chuva!</div></div>';
else if(r.rain_in_h>0)rain='<div class="c rain"><div class="k">&#9889; Alerta</div><div class="v warn">Chuva em '+r.rain_in_h+'h</div><div class="u">'+r.rain_prob+'% &middot; '+r.rain_mm+' mm</div></div>';
document.getElementById('g').innerHTML=
`<div class="c big"><div class="wxicon">${em(r.wcode)}</div><div><div class="v">${r.outdoor_temp.toFixed(1)}&deg;C</div><div class="u">${ds(r.wcode)} &middot; fora &middot; min ${r.tmin.toFixed(0)} / max ${r.tmax.toFixed(0)}</div></div></div>`
+rain+
`<div class="c"><div class="k">BMP581 temperatura</div><div class="v" style="color:#6fd3ff">${r.bmp_temp.toFixed(1)}<span class="u">&deg;C</span></div><div class="u">${ok(r.bmp_ok)}</div></div>
<div class="c"><div class="k">Pressao local</div><div class="v" style="color:#b8a6ff">${r.bmp_press.toFixed(0)}<span class="u"> hPa</span></div><div class="u">nivel do mar ${r.press_msl} hPa</div></div>
<div class="c"><div class="k">Altitude</div><div class="v" style="color:#7ddf87">${r.altitude}<span class="u"> m</span></div><div class="u">barometrica</div></div>
<div class="c"><div class="k">Umidade relativa</div><div class="v" style="color:#ffd166">${r.humidity<0?'--':r.humidity}<span class="u">%</span></div><div class="u">Open-Meteo</div></div>
<div class="c"><div class="k">Giroscopio</div><div class="v" style="font-size:1em;color:#d0d8e8">${r.gyro.map(g=>g.toFixed(0)).join(' / ')}</div><div class="u">dps &middot; QMI8658 ${ok(r.qmi_ok)}</div></div>`;
document.getElementById('foot').innerHTML=`Previsao: ${ok(r.wx_desc!=='')} &middot; atualizada ha ${r.wx_age_min} min &middot; rede ${r.net}`;
ssid.value=r.loc?ssid.value:''; if(!ssid.value)ssid.value='';
}catch(e){document.getElementById('sub').textContent='dispositivo offline'}}
async function save(){
const b={ssid:ssid.value,pass:pass.value,lat:parseFloat(lat.value),lon:parseFloat(lon.value),tz:tz.value};
const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});
document.getElementById('msg').textContent=r.ok?'Salvo! Reinicie.':'Erro ao salvar';}
setInterval(up,5000);up();
</script></body></html>)HTML";

static String ipStr() { return st.apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }

void timeInit() {
  configTzTime(cfg.tz, "pool.ntp.org", "time.nist.gov");
}
void geoLocate() {
  HTTPClient h;
  h.setConnectTimeout(5000); h.setTimeout(5000);
  if (!h.begin("http://ip-api.com/json/?fields=status,lat,lon,timezone,city")) return;
  if (h.GET() == 200) {
    JsonDocument *dp = new JsonDocument();
    JsonDocument &d = *dp;
    bool ok = !deserializeJson(d, h.getString());
    if (ok && d["status"] == "success") {
        st.lat = d["lat"] | 0.0f;
        st.lon = d["lon"] | 0.0f;
        strlcpy(st.tzName, d["timezone"] | "UTC", sizeof(st.tzName));
        snprintf(st.locName, sizeof(st.locName), "%s", d["city"] | "Desconhecido");
        // usa geo do IP se usuário não fixou manual
        if (cfg.lat == 0 && cfg.lon == 0) {
          cfg.lat = st.lat; cfg.lon = st.lon;
          strlcpy(cfg.tz, st.tzName, sizeof(cfg.tz));
          strlcpy(cfg.city, st.locName, sizeof(cfg.city));
          cfgSave();
          configTzTime(cfg.tz, "pool.ntp.org", "time.nist.gov");
        }
        Serial.printf("[GEO] %s %.4f,%.4f tz=%s\n", st.locName, st.lat, st.lon, st.tzName);
    }
    delete dp;
  }
  h.end();
}
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient h;
  h.setConnectTimeout(3000); h.setTimeout(4000); // travar o loop o minimo possivel
  char url[512];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,precipitation,relative_humidity_2m,pressure_msl"
           "&hourly=precipitation_probability,precipitation,weather_code"
           "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&forecast_days=2&timezone=auto",
           cfg.lat, cfg.lon);
  if (!h.begin(url)) { Serial.println("[WX] begin falhou (url?)"); return; }
  int code = h.GET();
  if (code == 200) {
    JsonDocument *dp = new JsonDocument();
    JsonDocument &d = *dp;
    if (!deserializeJson(d, h.getString())) {
      st.outdoorTemp = d["current"]["temperature_2m"] | 0.0f;
      st.wcode = d["current"]["weather_code"] | -1;
      st.humidity = d["current"]["relative_humidity_2m"] | -1.0f;
      st.pressMsl = d["current"]["pressure_msl"] | 0.0f;
      st.tmax = d["daily"]["temperature_2m_max"][0] | 0.0f;
      st.tmin = d["daily"]["temperature_2m_min"][0] | 0.0f;
      strlcpy(st.wxDesc, wcodeDesc(st.wcode), sizeof(st.wxDesc));
      // ----- alerta de chuva: procura 1a hora com prob >= 40% nas proximas 12h
      st.rainProb = -1; st.rainInH = -1; st.rainMm = 0;
      time_t now = time(nullptr);
      struct tm tmv; localtime_r(&now, &tmv);
      int curHourUtc = tmv.tm_hour; // hora local == indice hourly (timezone=auto)
      JsonArray prob = d["hourly"]["precipitation_probability"];
      JsonArray mm = d["hourly"]["precipitation"];
      // acha indice da hora atual (hourly comeca 00:00 de hoje local)
      int startIdx = tmv.tm_hour;
      for (int i = startIdx; i < (int)prob.size() && i < startIdx + 12; i++) {
        int p = prob[i] | -1;
        if (p >= 40) {
          st.rainInH = i - startIdx;
          st.rainProb = p;
          st.rainMm = mm[i] | 0.0f;
          break;
        }
      }
      if (st.rainInH == 0) st.rainProb = st.rainProb; // agora
      st.wxValid = true;
      st.wxAgeMin = 0;
      Serial.printf("[WX] %.1fC %s | chuva: prob=%d%% em %dh (%.1fmm)\n",
                    st.outdoorTemp, st.wxDesc, st.rainProb, st.rainInH, st.rainMm);
    }
    delete dp;
  } else {
    Serial.printf("[WX] HTTP %d\n", code);
  }
  h.end();
}

void handleRoot() { server.send_P(200, "text/html", PAGE); }
void handleStatus() {
  JsonDocument d;
  char clk[40];
  time_t now = time(nullptr);
  struct tm tmv;
  localtime_r(&now, &tmv);
  strftime(clk, sizeof(clk), "%d/%m %H:%M:%S", &tmv);
  d["clock"] = clk;
  d["bmp_ok"] = st.bmp == ST_OK;
  d["qmi_ok"] = st.qmi == ST_OK;
  d["bmp_temp"] = st.bmpTemp;
  d["bmp_press"] = st.bmpPress;
  JsonArray g = d["gyro"].to<JsonArray>();
  g.add(round(st.gyro[0] * 10) / 10.0); g.add(round(st.gyro[1] * 10) / 10.0); g.add(round(st.gyro[2] * 10) / 10.0);
  d["outdoor_temp"] = st.outdoorTemp;
  d["tmin"] = st.tmin; d["tmax"] = st.tmax;
  d["wx_desc"] = st.wxDesc;
  d["wx_age_min"] = st.wxAgeMin;
  d["rain_prob"] = st.rainProb;
  d["rain_in_h"] = st.rainInH;
  d["rain_mm"] = (int)(st.rainMm * 10) / 10.0;
  d["humidity"] = st.humidity;
  d["altitude"] = (int)(st.altitude);
  d["press_msl"] = (int)st.pressMsl;
  d["lat"] = cfg.lat; d["lon"] = cfg.lon;
  d["tz"] = cfg.tz;
  d["loc"] = strlen(cfg.city) ? cfg.city : "manual";
  d["net"] = st.apMode ? "AP" : (WiFi.status() == WL_CONNECTED ? "WiFi" : "off");
  d["ip"] = ipStr();
  String o; serializeJson(d, o);
  server.send(200, "application/json", o);
}
void handleConfig() {
  if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"err\":\"sem corpo\"}"); return; }
  JsonDocument d;
  DeserializationError err = deserializeJson(d, server.arg("plain"));
  if (err) { server.send(400, "application/json", "{\"err\":\"json invalido\"}"); return; }
  if (d["ssid"].is<const char *>()) strlcpy(cfg.ssid, d["ssid"] | "", sizeof(cfg.ssid));
  if (d["pass"].is<const char *>()) strlcpy(cfg.pass, d["pass"] | "", sizeof(cfg.pass));
  if (!d["lat"].isNull()) cfg.lat = d["lat"] | 0.0f;
  if (!d["lon"].isNull()) cfg.lon = d["lon"] | 0.0f;
  if (d["tz"].is<const char *>()) strlcpy(cfg.tz, d["tz"] | cfg.tz, sizeof(cfg.tz));
  cfgSave();
  server.send(200, "application/json", "{\"ok\":true}");
}
void handleReboot() { server.send(200, "application/json", "{\"ok\":true}"); delay(300); ESP.restart(); }

void webStart() {
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/reboot", handleReboot);
  server.begin();
}

// tendencia de pressao (3h, amostra a cada 10min) — previsao LOCAL
#define PTREND_N 18
static float ptrendBuf[PTREND_N];
static int ptrendCnt = 0, ptrendHead = 0;
static uint32_t tLastPtrend = 0;
void ptrendSample() {
  if (st.bmp != ST_OK || st.bmpPress < 300) return;
  ptrendBuf[ptrendHead] = st.bmpPress;
  ptrendHead = (ptrendHead + 1) % PTREND_N;
  if (ptrendCnt < PTREND_N) ptrendCnt++;
}
// hPa/3h: <-1.5 chuva provavel, >+1.5 melhoria
float ptrendDelta() {
  if (ptrendCnt < 6) return 0; // precisa >= 1h de dados
  int oldest = (ptrendHead - ptrendCnt + PTREND_N) % PTREND_N;
  float span_h = (ptrendCnt - 1) * (10.0f / 60.0f);
  if (span_h < 0.5f) return 0;
  return (st.bmpPress - ptrendBuf[oldest]) / span_h * 3.0f;
}

bool sdOk = false;
uint32_t tLastSd = 0;
SPIClass sdSPI(HSPI); // SPI3_HOST dedicado — o display QSPI usa o SPI2, NAO compartilhar
uint32_t tLastSensor = 0, tLastUi = 0, tLastWx = 0, tLastMin = 0;
uint32_t tLastLog = 0, tLastSensorRetry = 0;
uint32_t bootMs;

void debugLog() {
  char b[160];
  time_t nowt = time(nullptr);
  struct tm tmv; localtime_r(&nowt, &tmv);
  snprintf(b, sizeof(b),
           "[ST] %02d:%02d BMP=%s %.1fC %.1fhPa ptrend=%+.1f WiFi=%s IP=%s WX=%s",
           tmv.tm_hour, tmv.tm_min,
           st.bmp == ST_OK ? "OK" : "FALHA", st.bmpTemp, st.bmpPress, ptrendDelta(),
           st.apMode ? "AP" : (WiFi.status() == WL_CONNECTED ? "OK" : "off"),
           ipStr().c_str(),
           st.wxValid ? "OK" : "--");
  Serial.println(b);
}

void startAP() {
  st.apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("EstacaoMeteo", "12345678");
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("[AP] SSID EstacaoMeteo IP %s\n", WiFi.softAPIP().toString().c_str());
}

void netStart() {
  wifiMulti.addAP(cfg.ssid, cfg.pass);
  WiFi.mode(WIFI_STA);
  Serial.printf("[WiFi] conectando a %s", cfg.ssid);
  uint32_t t0 = millis();
  while (wifiMulti.run() != WL_CONNECTED && millis() - t0 < 15000) { delay(300); Serial.print("."); }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] falhou — abrindo AP de configuracao");
    startAP();
  } else {
    Serial.printf("[WiFi] OK IP %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin("estacao")) MDNS.addService("http", "tcp", 80);
  }
  webStart();
}

// ============================ UI refresh =====================================
static void uiTick() {
  time_t now = time(nullptr);
  struct tm tmv;
  localtime_r(&now, &tmv);
  char b[64];

  static const char *dias[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
  static const char *meses[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
  snprintf(b, sizeof(b), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
  lv_label_set_text(lblClock, b);
  snprintf(b, sizeof(b), "%s, %d de %s", dias[tmv.tm_wday], tmv.tm_mday, meses[tmv.tm_mon]);
  lv_label_set_text(lblDate, b);

  if (st.wxValid) {
    snprintf(b, sizeof(b), "%.1f C", st.outdoorTemp);
    lv_label_set_text(lblWxTemp, b);
    lv_label_set_text(lblWxDesc, st.wxDesc);
    float pd = ptrendDelta();
    const char *trend = "";
    if (pd < -1.5f) trend = " | pressao caindo: chuva provavel";
    else if (pd > 1.5f) trend = " | pressao subindo: tempo firma";
    snprintf(b, sizeof(b), "min %.0f  max %.0f%s", st.tmin, st.tmax, trend);
    lv_label_set_text(lblWxMini, b);
    lv_image_set_src(imgWx, wcodeIcon(st.wcode));
    // alerta de chuva
    if (st.rainInH >= 0) {
      lv_obj_clear_flag(cRain, LV_OBJ_FLAG_HIDDEN);
      if (st.rainInH == 0)
        snprintf(b, sizeof(b), "CHOVENDO AGORA (%.1f mm) — leva guarda-chuva!", st.rainMm);
      else if (st.rainInH == 1)
        snprintf(b, sizeof(b), "Chuva na proxima hora (%d%%, %.1f mm) — leva guarda-chuva!", st.rainProb, st.rainMm);
      else
        snprintf(b, sizeof(b), "Chuva em %dh (%d%%, %.1f mm) — leva guarda-chuva!", st.rainInH, st.rainProb, st.rainMm);
      lv_label_set_text(lblRainAlert, b);
    } else {
      lv_obj_add_flag(cRain, LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    lv_label_set_text(lblWxTemp, "--");
    lv_label_set_text(lblWxDesc, WiFi.status() == WL_CONNECTED ? "carregando..." : "sem rede");
    lv_obj_add_flag(cRain, LV_OBJ_FLAG_HIDDEN);
  }

  if (st.bmp == ST_OK) {
    snprintf(b, sizeof(b), "%.1f C", st.bmpTemp);
    lv_label_set_text(lblBmpT, b);
  } else {
    lv_label_set_text(lblBmpT, st.bmp == ST_FAIL ? "BMP581 x" : "BMP581...");
  }
  snprintf(b, sizeof(b), "%.0f hPa", st.bmpPress);
  lv_label_set_text(lblBmpP, b);
  if (st.bmp == ST_OK) {
    float msl = st.pressMsl > 0 ? st.pressMsl : 1013.25f;
    st.altitude = 44330.0f * (1.0f - powf(st.bmpPress / msl, 0.1902949572f));
    snprintf(b, sizeof(b), "alt %.0f m", st.altitude);
    lv_label_set_text(lblAlt, b);
  }
  if (st.humidity >= 0) {
    snprintf(b, sizeof(b), "umid %.0f%%", st.humidity);
    lv_label_set_text(lblHum, b);
  }
  snprintf(b, sizeof(b), "Fora: %.1f C  %.0f%%", st.outdoorTemp, st.humidity < 0 ? 0 : st.humidity);
  lv_label_set_text(lblWxOut, b);

  snprintf(b, sizeof(b), "BMP581 %s", st.bmp == ST_OK ? "OK" : (st.bmp == ST_FAIL ? "FALHA" : "..."));
  lv_label_set_text(lblStBmp, b);
  snprintf(b, sizeof(b), "QMI8658 %s", st.qmi == ST_OK ? "OK" : "FALHA");
  lv_label_set_text(lblStQmi, b);
  snprintf(b, sizeof(b), "Previsao %s", st.wxValid ? "OK" : "--");
  lv_label_set_text(lblStWx, b);
  if (st.apMode) snprintf(b, sizeof(b), "AP 192.168.4.1");
  else {
    String ip = ipStr();
    snprintf(b, sizeof(b), "IP %s", ip.c_str());
  }
  lv_label_set_text(lblStNet, b);

  snprintf(b, sizeof(b),
           "WiFi: %s\nIP: %s\nLocal: %s\nFuso: %s\n\nPara editar WiFi/local/fuso,\nacesse http://%s/\nno navegador do celular ou PC.",
           st.apMode ? "AP (EstacaoMeteo)" : cfg.ssid,
           ipStr().c_str(),
           strlen(cfg.city) ? cfg.city : "manual",
           cfg.tz, ipStr().c_str());
  lv_label_set_text(lblCfgInfo, b);

  if (st.qmi == ST_OK) {
    snprintf(b, sizeof(b), "Giro: %+.0f %+.0f %+.0f dps", st.gyro[0], st.gyro[1], st.gyro[2]);
    lv_label_set_text(lblGyro, b);
    snprintf(b, sizeof(b), "Acc: %+.2f %+.2f %+.2f g", st.acc[0], st.acc[1], st.acc[2]);
    lv_label_set_text(lblAcc, b);
    snprintf(b, sizeof(b), "Rotacao: %d (%s)", st.rot, scrW > scrH ? "paisagem" : "retrato");
    lv_label_set_text(lblRot, b);
  }
}

// ============================ setup / loop ===================================
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("\n=== Estacao Meteo — ESP32-S3-Touch-AMOLED-1.64 ===");

  cfgLoad();

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(I2C_HZ);
  Wire.setTimeOut(50);

  // display
  if (!gfx->begin()) Serial.println("[GFX] begin falhou!");
  gfx->fillScreen(RGB565_BLACK);
  scrW = gfx->width(); scrH = gfx->height();
  Serial.printf("[GFX] %ux%u rot=%d\n", scrW, scrH, st.rot);

  lv_init();
  lv_tick_set_cb(millis_cb);
  bufPx = scrW * 30;
  dbuf = (lv_color_t *)heap_caps_aligned_alloc(4, bufPx * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  disp = lv_display_create(scrW, scrH);
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180); // placa montada 180° — software, seguro
  lv_display_set_flush_cb(disp, disp_flush);
  lv_display_set_buffers(disp, dbuf, NULL, bufPx * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(disp, disp_rounder, LV_EVENT_INVALIDATE_AREA, NULL);

  { // diagnostico FT3168
    Wire.beginTransmission(FT_ADDR);
    uint8_t pr = Wire.endTransmission();
    Serial.printf("[FT] probe 0x38: %s\n", pr == 0 ? "presente" : "AUSENTE");
    Wire.beginTransmission(FT_ADDR); Wire.write((uint8_t)0xA8); Wire.endTransmission(false);
    if (Wire.requestFrom(FT_ADDR, 1) == 1) Serial.printf("[FT] chip_id=0x%02X\n", Wire.read());
  }
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);

  uiInit();
  layoutUi();

  // sensores
  st.bmp = bmp.begin(BMP_ADDR, &Wire) ? ST_OK : ST_FAIL;
  if (st.bmp == ST_OK) {
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_8X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_15);
    bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
    Serial.println("[BMP] OK");
  } else Serial.println("[BMP] falhou");
  st.qmi = qmiInit() ? ST_OK : ST_FAIL;

  // SD card (GPIO38 CS, 39 MOSI, 40 MISO, 41 CLK) em host SEPARADO do display
  sdSPI.begin(41 /*CLK*/, 40 /*MISO*/, 39 /*MOSI*/, 38 /*SS*/);
  if (SD.begin(38, sdSPI, 4000000)) {
    sdOk = true;
    Serial.println("[SD] ok");
    File f = SD.open("/estacao.csv", FILE_APPEND);
    if (f && f.size() == 0) { f.println("epoch,temp_c,press_hpa,alt_m,hum_pct,out_temp_c,rain_prob,rain_in_h"); }
    if (f) f.close();
  } else Serial.println("[SD] ausente (segue sem log)");

  netStart();
  if (!st.apMode) {
    timeInit();
    geoLocate(); // também define lat/lon/tz se não fixados
    fetchWeather();
  }

  bootMs = millis();
  tLastPtrend = millis() - 570000; // 1a amostra ~3min apos boot
  Serial.println("[RUN] loop");
}

void loop() {
  uint32_t ms = millis();

  if (st.apMode) dns.processNextRequest();
  server.handleClient();

  if (ms - tLastSensor >= 200) {
    tLastSensor = ms;
    if (st.qmi == ST_OK) qmiRead();
    static uint32_t tLastBmp = 0;
    if (st.bmp == ST_OK && ms - tLastBmp >= 1000) { tLastBmp = ms; bmpRead(); }

    // retry de sensores que falharam no boot (ex.: plugados depois)
    if ((st.bmp == ST_FAIL || st.qmi == ST_FAIL) && ms - tLastSensorRetry >= 5000) {
      tLastSensorRetry = ms;
      if (st.bmp == ST_FAIL && bmp.begin(BMP_ADDR, &Wire)) {
        bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_8X);
        bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_8X);
        bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_15);
        bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
        st.bmp = ST_OK;
        Serial.println("[BMP] encontrado no retry!");
      }
      if (st.qmi == ST_FAIL && qmiInit()) { st.qmi = ST_OK; Serial.println("[QMI] encontrado no retry!"); }
    }

    // rotação automatica DESATIVADA no painel (CO5300 nao aceita setRotation
    // em runtime — corrompe a tela). rot fixo 0.
    st.rot = 2;
  }

  if (ms - tLastUi >= 500) { tLastUi = ms; uiTick(); }
  if (ms - tLastPtrend >= 600000UL) { tLastPtrend = ms; ptrendSample(); } // 10min

  if (sdOk && ms - tLastSd >= 60000) {
    tLastSd = ms;
    File f = SD.open("/estacao.csv", FILE_APPEND);
    if (f) {
      char line[128];
      snprintf(line, sizeof(line), "%ld,%.1f,%.1f,%.0f,%.0f,%.1f,%d,%d",
               (long)time(nullptr), st.bmpTemp, st.bmpPress, st.altitude,
               st.humidity < 0 ? -1 : st.humidity, st.outdoorTemp, st.rainProb, st.rainInH);
      f.println(line);
      f.close();
    }
  }
  if (ms - tLastLog >= 10000) { tLastLog = ms; debugLog(); }

  if (!st.apMode) {
    if (ms - tLastWx >= 30UL * 60 * 1000) { tLastWx = ms; fetchWeather(); geoLocate(); }
    if (WiFi.status() != WL_CONNECTED && ms - bootMs > 60000) {
      static uint32_t tReconn = 0;
      if (ms - tReconn > 30000) { tReconn = ms; wifiMulti.run(); }
    }
    if (time(nullptr) < 1600000000 || difftime(time(nullptr), lastNtpSync) > 3600) {
      lastNtpSync = time(nullptr);
    }
  }

  uint32_t delay_ms = lv_timer_handler();
  delay(delay_ms < 5 ? 5 : (delay_ms > 20 ? 20 : delay_ms));
}
