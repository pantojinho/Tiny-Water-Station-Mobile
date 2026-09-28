#include <Arduino.h>
/*
 * Estação Meteorológica Mini
 * Waveshare ESP32-S3-Touch-AMOLED-1.64 (CO5300 QSPI 280x456 + FT3168 touch + QMI8658 IMU)
 * Sensor externo: Adafruit BMP581 (I2C 0x47 no barramento GPIO47/48)
 *
 * - Relógio NTP + data (fuso via geo-IP, ajustável pela web)
 * - Previsão/tempo atual via Open-Meteo (localização via geo-IP ou manual na web)
 * - BMP581: temperatura/pressão locais; QMI8658: giro/acel
 * - Tela fixa, sem touch, com endereço para configuração pelo navegador
 * - Modo AP automático se o Wi-Fi salvo não conectar ou cair por 60 s
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
#define WIFI_SSID_DEFAULT ""
#define WIFI_PASS_DEFAULT ""

// ============================ Pinos / hardware ===============================
#define TOUCH_SDA 47
#define TOUCH_SCL 48
#define I2C_HZ 100000

#define QMI_ADDR 0x6B
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
  char city[96];
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
  bool isDay = true;
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
static volatile uint32_t lastWxOkMs = 0;
static volatile uint32_t locationVersion = 0;
static volatile bool requestApMode = false;

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

// ============================ Tela somente leitura ==========================
// A tela não tem áreas clicáveis. Toda a configuração é feita pelo navegador.
static lv_obj_t *lblClock, *lblDate, *lblWxTemp, *lblWxDesc, *lblWxMini;
static lv_obj_t *lblBmpT, *lblBmpP, *lblAlt, *lblHum;
static lv_obj_t *lblNetMode, *lblNetMain, *lblNetHint, *lblNetHelp;
static lv_obj_t *imgWx;
static lv_style_t stCard;

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

static lv_obj_t *makeCard(lv_obj_t *parent, int y, int height) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_add_style(card, &stCard, 0);
  lv_obj_set_pos(card, 8, y);
  lv_obj_set_size(card, 264, height);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

static lv_obj_t *mkLabel(lv_obj_t *parent, const lv_font_t *font,
                         uint32_t color, int x, int y, int width) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(label, "--");
  return label;
}

static void uiInit() {
  lv_style_init(&stCard);
  lv_style_set_bg_color(&stCard, lv_color_hex(0x141c2b));
  lv_style_set_border_color(&stCard, lv_color_hex(0x304159));
  lv_style_set_border_width(&stCard, 1);
  lv_style_set_radius(&stCard, 12);
  lv_style_set_pad_all(&stCard, 0);

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x09111f), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Cada linha ocupa uma área fixa; nenhuma atualização reposiciona textos.
  lv_obj_t *clockCard = makeCard(scr, 8, 64);
  lblClock = mkLabel(clockCard, &lv_font_montserrat_28, 0xffffff, 12, 6, 235);
  lblDate = mkLabel(clockCard, &lv_font_montserrat_12, 0x9db2cc, 12, 43, 235);

  lv_obj_t *weatherCard = makeCard(scr, 80, 108);
  imgWx = lv_image_create(weatherCard);
  lv_image_set_src(imgWx, &icon_cloud);
  lv_obj_set_pos(imgWx, 8, 18);
  lblWxTemp = mkLabel(weatherCard, &lv_font_montserrat_20, 0xffd174, 90, 10, 160);
  lblWxDesc = mkLabel(weatherCard, &lv_font_montserrat_14, 0xe7eefb, 90, 45, 160);
  lblWxMini = mkLabel(weatherCard, &lv_font_montserrat_12, 0x9db2cc, 90, 80, 160);

  lv_obj_t *sensorCard = makeCard(scr, 196, 100);
  lblBmpT = mkLabel(sensorCard, &lv_font_montserrat_20, 0x70dff4, 12, 10, 116);
  lblBmpP = mkLabel(sensorCard, &lv_font_montserrat_20, 0xc4a8ff, 130, 10, 125);
  lblAlt = mkLabel(sensorCard, &lv_font_montserrat_14, 0xa1e7a7, 12, 62, 116);
  lblHum = mkLabel(sensorCard, &lv_font_montserrat_14, 0xffcf78, 130, 62, 125);

  lv_obj_t *networkCard = makeCard(scr, 304, 144);
  lv_obj_set_style_bg_color(networkCard, lv_color_hex(0x14344a), 0);
  lblNetMode = mkLabel(networkCard, &lv_font_montserrat_14, 0xa7dce8, 12, 12, 237);
  lblNetMain = mkLabel(networkCard, &lv_font_montserrat_20, 0xffffff, 12, 40, 237);
  lblNetHint = mkLabel(networkCard, &lv_font_montserrat_14, 0xd8e7f4, 12, 76, 237);
  lblNetHelp = mkLabel(networkCard, &lv_font_montserrat_12, 0xa7dce8, 12, 108, 237);
}

// ============================ WiFi / hora / previsão =========================
WiFiMulti wifiMulti;
WebServer server(80);
DNSServer dns;

#include "web_page.h"

static String ipStr() { return st.apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }

static const char *posixTz(const char *name) {
  // newlib no ESP32 espera regras POSIX, nao identificadores IANA.
  if (!strcmp(name, "America/Sao_Paulo") || !strcmp(name, "America/Fortaleza") ||
      !strcmp(name, "America/Recife") || !strcmp(name, "America/Bahia") ||
      !strcmp(name, "America/Belem")) return "BRT3";
  if (!strcmp(name, "America/Manaus") || !strcmp(name, "America/Cuiaba") ||
      !strcmp(name, "America/Porto_Velho") || !strcmp(name, "America/Boa_Vista")) return "AMT4";
  if (!strcmp(name, "America/Rio_Branco") || !strcmp(name, "America/Eirunepe")) return "ACT5";
  if (!strcmp(name, "America/Noronha")) return "FNT2";
  if (!strcmp(name, "UTC")) return "UTC0";
  return name; // aceita regras POSIX informadas diretamente na configuracao
}
void timeInit() {
  configTzTime(posixTz(cfg.tz), "pool.ntp.org", "time.nist.gov");
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
          timeInit();
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
           "&current=temperature_2m,weather_code,is_day,precipitation,relative_humidity_2m,pressure_msl"
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
      st.isDay = (d["current"]["is_day"] | 1) != 0;
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
      lastWxOkMs = millis();
      Serial.printf("[WX] %.1fC %s | chuva: prob=%d%% em %dh (%.1fmm)\n",
                    st.outdoorTemp, st.wxDesc, st.rainProb, st.rainInH, st.rainMm);
    }
    delete dp;
  } else {
    Serial.printf("[WX] HTTP %d\n", code);
  }
  h.end();
}

// HTTP pode esperar varios segundos; manter estas consultas fora do loop do LVGL.
static void weatherTask(void *) {
  timeInit();
  uint32_t lastAttempt = 0;
  uint32_t lastReconnect = 0;
  uint32_t offlineSince = 0;
  uint32_t seenLocationVersion = locationVersion;
  for (;;) {
    uint32_t now = millis();
    if (seenLocationVersion != locationVersion) {
      seenLocationVersion = locationVersion;
      timeInit();
      lastAttempt = 0;
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (offlineSince == 0) offlineSince = now;
      if (lastReconnect == 0 || now - lastReconnect >= 10000) {
        lastReconnect = now;
        wifiMulti.run();
      }
      if (WiFi.status() != WL_CONNECTED && millis() - offlineSince >= 60000UL) {
        Serial.println("[WiFi] sem conexao por 60 s; abrindo AP");
        requestApMode = true;
        vTaskDelete(nullptr);
      }
    } else offlineSince = 0;
    if (WiFi.status() == WL_CONNECTED &&
        (lastAttempt == 0 || now - lastAttempt >= 30UL * 60 * 1000)) {
      lastAttempt = now;
      if (cfg.lat == 0 && cfg.lon == 0) geoLocate();
      if (cfg.lat != 0 || cfg.lon != 0) fetchWeather();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
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
  d["wcode"] = st.wcode;
  d["is_day"] = st.isDay;
  d["wx_age_min"] = lastWxOkMs ? (millis() - lastWxOkMs) / 60000UL : 999;
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
  if ((!d["lat"].isNull() && !d["lat"].is<float>() && !d["lat"].is<double>() && !d["lat"].is<int>()) ||
      (!d["lon"].isNull() && !d["lon"].is<float>() && !d["lon"].is<double>() && !d["lon"].is<int>()) ||
      (!d["lat"].isNull() && (d["lat"].as<float>() < -90 || d["lat"].as<float>() > 90)) ||
      (!d["lon"].isNull() && (d["lon"].as<float>() < -180 || d["lon"].as<float>() > 180))) {
    server.send(400, "application/json", "{\"err\":\"coordenadas invalidas\"}"); return;
  }
  bool locationChanged = false;
  if (!d["lat"].isNull() && cfg.lat != d["lat"].as<float>()) locationChanged = true;
  if (!d["lon"].isNull() && cfg.lon != d["lon"].as<float>()) locationChanged = true;
  if (d["tz"].is<const char *>() && strcmp(cfg.tz, d["tz"].as<const char *>())) locationChanged = true;
  if (d["city"].is<const char *>() && strcmp(cfg.city, d["city"].as<const char *>())) locationChanged = true;
  if (d["ssid"].is<const char *>()) strlcpy(cfg.ssid, d["ssid"] | "", sizeof(cfg.ssid));
  if (d["pass"].is<const char *>() && strlen(d["pass"].as<const char *>()) > 0)
    strlcpy(cfg.pass, d["pass"].as<const char *>(), sizeof(cfg.pass));
  if (!d["lat"].isNull()) cfg.lat = d["lat"] | 0.0f;
  if (!d["lon"].isNull()) cfg.lon = d["lon"] | 0.0f;
  if (d["tz"].is<const char *>()) strlcpy(cfg.tz, d["tz"] | cfg.tz, sizeof(cfg.tz));
  if (d["city"].is<const char *>()) strlcpy(cfg.city, d["city"] | "", sizeof(cfg.city));
  cfgSave();
  if (locationChanged) {
    st.wxValid = false;
    st.wxDesc[0] = 0;
    lastWxOkMs = 0;
    ++locationVersion;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}
void handleGetConfig() {
  JsonDocument d;
  d["ssid"] = cfg.ssid;
  d["lat"] = cfg.lat;
  d["lon"] = cfg.lon;
  d["tz"] = cfg.tz;
  d["city"] = cfg.city;
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}
void handleReboot() { server.send(200, "application/json", "{\"ok\":true}"); delay(300); ESP.restart(); }

void webStart() {
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
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
uint32_t tLastSensor = 0, tLastUi = 0;
uint32_t tLastLog = 0, tLastSensorRetry = 0;

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

static void uiTick();

void startAP() {
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP("EstacaoMeteo", "12345678")) {
    Serial.println("[AP] falha ao iniciar; tentando novamente");
    return;
  }
  st.apMode = true;
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("[AP] SSID EstacaoMeteo IP %s\n", WiFi.softAPIP().toString().c_str());
}

void netStart() {
  if (!cfg.ssid[0]) {
    Serial.println("[WiFi] rede nao configurada; abrindo AP");
    startAP();
    webStart();
    return;
  }
  wifiMulti.addAP(cfg.ssid, cfg.pass);
  WiFi.mode(WIFI_STA);
  Serial.printf("[WiFi] conectando a %s", cfg.ssid);
  uint32_t t0 = millis();
  while (wifiMulti.run() != WL_CONNECTED && millis() - t0 < 20000) {
    uiTick();
    lv_timer_handler();
    delay(300);
    Serial.print(".");
  }
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

// ============================ Atualização da tela ===========================
static void setLabelIfChanged(lv_obj_t *label, const char *value) {
  if (strcmp(lv_label_get_text(label), value) != 0) lv_label_set_text(label, value);
}

static void uiTick() {
  time_t now = time(nullptr);
  struct tm tmv;
  localtime_r(&now, &tmv);
  char b[64];
  static const char *dias[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
  static const char *meses[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
  if (tmv.tm_year >= 120) {
    snprintf(b, sizeof(b), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    setLabelIfChanged(lblClock, b);
    snprintf(b, sizeof(b), "%s, %d de %s", dias[tmv.tm_wday], tmv.tm_mday, meses[tmv.tm_mon]);
    setLabelIfChanged(lblDate, b);
  } else {
    setLabelIfChanged(lblClock, "--:--");
    setLabelIfChanged(lblDate, "Aguardando horario");
  }

  if (st.wxValid) {
    snprintf(b, sizeof(b), "%.1f C", st.outdoorTemp);
    setLabelIfChanged(lblWxTemp, b);
    setLabelIfChanged(lblWxDesc, wcodeDesc(st.wcode));
    snprintf(b, sizeof(b), "min %.0f / max %.0f", st.tmin, st.tmax);
    setLabelIfChanged(lblWxMini, b);
    const lv_image_dsc_t *icon = wcodeIcon(st.wcode);
    if (lv_image_get_src(imgWx) != icon) lv_image_set_src(imgWx, icon);
  } else {
    setLabelIfChanged(lblWxTemp, "Tempo externo");
    setLabelIfChanged(lblWxDesc, st.apMode ? "Sem internet" : "Aguardando...");
    setLabelIfChanged(lblWxMini, "Previsao via web");
    if (lv_image_get_src(imgWx) != &icon_cloud) lv_image_set_src(imgWx, &icon_cloud);
  }

  if (st.bmp == ST_OK) {
    snprintf(b, sizeof(b), "%.1f C", st.bmpTemp);
    setLabelIfChanged(lblBmpT, b);
    snprintf(b, sizeof(b), "%.0f hPa", st.bmpPress);
    setLabelIfChanged(lblBmpP, b);
    float msl = st.pressMsl > 0 ? st.pressMsl : 1013.25f;
    st.altitude = 44330.0f * (1.0f - powf(st.bmpPress / msl, 0.1902949572f));
    snprintf(b, sizeof(b), "alt %.0f m", st.altitude);
    setLabelIfChanged(lblAlt, b);
  } else {
    setLabelIfChanged(lblBmpT, "-- C");
    setLabelIfChanged(lblBmpP, "-- hPa");
    setLabelIfChanged(lblAlt, "alt -- m");
  }
  if (st.humidity >= 0 && !st.apMode) {
    snprintf(b, sizeof(b), "umid %.0f%%", st.humidity);
    setLabelIfChanged(lblHum, b);
  } else setLabelIfChanged(lblHum, "umid --%");

  if (st.apMode) {
    setLabelIfChanged(lblNetMode, "CONFIGURAR WI-FI");
    setLabelIfChanged(lblNetMain, "EstacaoMeteo");
    setLabelIfChanged(lblNetHint, "Senha: 12345678");
    setLabelIfChanged(lblNetHelp, "Abra: http://192.168.4.1/");
  } else if (WiFi.status() == WL_CONNECTED) {
    setLabelIfChanged(lblNetMode, "ACESSE PELO NAVEGADOR");
    String ip = WiFi.localIP().toString();
    setLabelIfChanged(lblNetMain, ip.c_str());
    snprintf(b, sizeof(b), "http://%s/", ip.c_str());
    setLabelIfChanged(lblNetHint, b);
    setLabelIfChanged(lblNetHelp, "Wi-Fi conectado | Config. pela web");
  } else {
    setLabelIfChanged(lblNetMode, "WI-FI INDISPONIVEL");
    setLabelIfChanged(lblNetMain, "Conectando...");
    setLabelIfChanged(lblNetHint, "Aguarde o modo de configuracao");
    setLabelIfChanged(lblNetHelp, "O AP sera aberto automaticamente");
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

  uiInit(); // tela fixa, sem touch (UI dele e estatica; layoutUi nao existe mais)

  // sensores
  st.bmp = bmp.begin(BMP_ADDR, &Wire) ? ST_OK : ST_FAIL;
  if (st.bmp == ST_OK) {
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_8X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_15);
    bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
    bmpRead();
    Serial.println("[BMP] OK");
  } else Serial.println("[BMP] falhou");
  st.qmi = qmiInit() ? ST_OK : ST_FAIL;
  if (st.qmi == ST_OK) qmiRead();

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
    if (xTaskCreatePinnedToCore(weatherTask, "weather", 16384, nullptr, 1, nullptr, 0) != pdPASS)
      Serial.println("[WX] sem memoria para tarefa de rede");
  }

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
    if (time(nullptr) < 1600000000 || difftime(time(nullptr), lastNtpSync) > 3600) {
      lastNtpSync = time(nullptr);
    }
  }

  uint32_t delay_ms = lv_timer_handler();
  delay(delay_ms < 5 ? 5 : (delay_ms > 20 ? 20 : delay_ms));
}
