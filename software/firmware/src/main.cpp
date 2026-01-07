/**
 * @file main.cpp
 * @brief Firmware principal para Tiny Water Station Mobile
 * 
 * Este firmware controla todos os sensores, comunicação LoRa, GPS,
 * display LCD e datalogger para a Tiny Water Station Mobile.
 * 
 * Hardware: ESP32-S3-WROOM-1-N16R8
 * Versão: 2.1.0
 * Framework: Arduino
 * 
 * @author CIANDRINI
 * @date 2026
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <Adafruit_BME680.h>
#include <RadioLib.h>

// ==================== Configurações ====================

// Debug
#define DEBUG_LEVEL 2  // 0: Off, 1: Error, 2: Info, 3: Debug
#ifdef DEBUG_LEVEL
  #define DBG_PRINT(level, fmt, ...) \
    if (DEBUG_LEVEL >= level) { \
      Serial.printf(fmt, ##__VA_ARGS__); \
    }
#else
  #define DBG_PRINT(level, fmt, ...)
#endif

// I2C - Barramento de sensores
#ifndef I2C_SDA
  #define I2C_SDA 47
#endif
#ifndef I2C_SCL
  #define I2C_SCL 48
#endif

// UART - GPS
#ifndef GPS_RX
  #define GPS_RX 17
#endif
#ifndef GPS_TX
  #define GPS_TX 18
#endif

// SPI - Compartilhado
#define SPI_MOSI 11
#define SPI_MISO 13
#define SPI_SCK 12

// Seletores de Chip (CS)
#ifndef SD_CS
  #define SD_CS 21
#endif
#ifndef LCD_CS
  #define LCD_CS 9
#endif
#ifndef LCD_DC
  #define LCD_DC 15
#endif
#ifndef LORA_CS
  #define LORA_CS 10
#endif
#ifndef LORA_DIO1
  #define LORA_DIO1 14
#endif

// ==================== Objetos Globais ====================

// Sensores
Adafruit_BME680 bme688;  // Sensor de ambiente (temp, umid, press, gas)

// Comunicação
TinyGPSPlus gps;  // Parser GPS

// LoRa
SX1262 lora = new Module(LORA_CS, LORA_DIO1, -1, -1);

// Arquivos
File logFile;

// Estrutura de dados dos sensores
struct SensorData {
  // GPS
  float latitude = 0.0;
  float longitude = 0.0;
  float altitude = 0.0;
  float velocity = 0.0;
  bool gpsFix = false;
  
  // BME688 - Ambiente
  float temperature = 0.0;
  float humidity = 0.0;
  float pressure = 0.0;
  float gasResistance = 0.0;
  
  // Sistema
  float batteryPercent = 0.0;
  uint32_t timestamp = 0;
  int16_t loraRSSI = 0;
};

SensorData sensorData;

// ==================== Funções Auxiliares ====================

/**
 * @brief Inicializa o barramento I2C
 */
void initI2C() {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);  // 400kHz Fast Mode
  DBG_PRINT(2, "[I2C] Inicializado (SDA=%d, SCL=%d)\n", I2C_SDA, I2C_SCL);
}

/**
 * @brief Verifica se um sensor I2C está presente
 * @param address Endereço I2C do sensor
 * @param name Nome do sensor para debug
 * @return true se sensor detectado, false caso contrário
 */
bool checkI2CDevice(uint8_t address, const char* name) {
  Wire.beginTransmission(address);
  uint8_t error = Wire.endTransmission();
  
  if (error == 0) {
    DBG_PRINT(2, "[I2C] Sensor %s detectado em 0x%02X\n", name, address);
    return true;
  } else {
    DBG_PRINT(1, "[I2C] Sensor %s NÃO detectado em 0x%02X (erro=%d)\n", name, address, error);
    return false;
  }
}

/**
 * @brief Inicializa o sensor BME688
 */
bool initBME688() {
  if (!bme688.begin(0x77)) {  // Endereço padrão I2C
    DBG_PRINT(1, "[BME688] Falha na inicialização\n");
    return false;
  }
  
  // Configuração do BME688
  bme688.setTemperatureOversampling(BME680_OS_8X);
  bme688.setHumidityOversampling(BME680_OS_2X);
  bme688.setPressureOversampling(BME680_OS_4X);
  bme688.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme688.setGasHeater(320, 150);  // 320°C por 150ms
  
  DBG_PRINT(2, "[BME688] Inicializado com sucesso\n");
  return true;
}

/**
 * @brief Inicializa o módulo LoRa
 */
bool initLoRa() {
  // Inicializa o módulo LoRa
  int state = lora.begin(915.0);  // Frequência 915 MHz
  
  if (state == RADIOLIB_ERR_NONE) {
    DBG_PRINT(2, "[LoRa] Inicializado com sucesso (915 MHz)\n");
    
    // Configuração adicional
    lora.setBandwidth(125.0);
    lora.setSpreadingFactor(9);
    lora.setCodingRate(7);
    lora.setOutputPower(17);
    
    return true;
  } else {
    DBG_PRINT(1, "[LoRa] Falha na inicialização (erro=%d)\n", state);
    return false;
  }
}

/**
 * @brief Inicializa o cartão SD
 */
bool initSD() {
  if (!SD.begin(SD_CS, SPI, 25000000)) {  // 25MHz max clock
    DBG_PRINT(1, "[SD] Falha na inicialização\n");
    return false;
  }
  
  DBG_PRINT(2, "[SD] Inicializado com sucesso\n");
  DBG_PRINT(2, "[SD] Tipo de cartão: ");
  switch (SD.cardType()) {
    case CARD_MMC: DBG_PRINT(2, "MMC\n"); break;
    case CARD_SD:  DBG_PRINT(2, "SD\n"); break;
    case CARD_SDHC: DBG_PRINT(2, "SDHC\n"); break;
    default: DBG_PRINT(2, "UNKNOWN\n");
  }
  
  return true;
}

/**
 * @brief Inicializa o display LCD
 */
bool initDisplay() {
  DBG_PRINT(2, "[Display] Inicializando...\n");
  // TODO: Implementar inicialização do display LCD IPS 1.14"
  // Usar biblioteca TFT_eSPI ou ST7735
  DBG_PRINT(2, "[Display] Inicializado com sucesso\n");
  return true;
}

/**
 * @brief Inicializa o GPS
 */
void initGPS() {
  Serial2.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  DBG_PRINT(2, "[GPS] Inicializado (RX=%d, TX=%d)\n", GPS_RX, GPS_TX);
}

/**
 * @brief Lê dados do GPS
 */
void readGPS() {
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }
  
  if (gps.location.isValid()) {
    sensorData.latitude = gps.location.lat();
    sensorData.longitude = gps.location.lng();
    sensorData.altitude = gps.altitude.meters();
    sensorData.velocity = gps.speed.kmph();
    sensorData.gpsFix = true;
  } else {
    sensorData.gpsFix = false;
  }
}

/**
 * @brief Lê dados do BME688
 */
void readBME688() {
  if (!bme688.performReading()) {
    DBG_PRINT(1, "[BME688] Falha na leitura\n");
    return;
  }
  
  sensorData.temperature = bme688.temperature;
  sensorData.humidity = bme688.humidity;
  sensorData.pressure = bme688.pressure / 100.0;  // hPa
  sensorData.gasResistance = bme688.gas_resistance / 1000.0;  // kOhms
}

/**
 * @brief Atualiza todos os sensores
 */
void updateSensors() {
  readGPS();
  readBME688();
  
  // TODO: Implementar leitura de outros sensores:
  // - BMP581 (pressão de alta precisão)
  // - BMA400 (acelerômetro)
  // - BMM350 (magnetômetro)
  // - LTR-390 (UV e luz)
  // - MAX17048 (bateria)
  
  sensorData.timestamp = millis();
}

/**
 * @brief Transmite dados via LoRa
 */
void transmitLoRa() {
  // Cria payload em formato JSON
  String payload = "{";
  payload += "\"lat\":" + String(sensorData.latitude, 6) + ",";
  payload += "\"lon\":" + String(sensorData.longitude, 6) + ",";
  payload += "\"alt\":" + String(sensorData.altitude, 1) + ",";
  payload += "\"temp\":" + String(sensorData.temperature, 1) + ",";
  payload += "\"hum\":" + String(sensorData.humidity, 1) + ",";
  payload += "\"pres\":" + String(sensorData.pressure, 1);
  payload += "}";
  
  int state = lora.transmit(payload.c_str());
  
  if (state == RADIOLIB_ERR_NONE) {
    DBG_PRINT(3, "[LoRa] Payload transmitido: %s\n", payload.c_str());
  } else {
    DBG_PRINT(1, "[LoRa] Falha na transmissão (erro=%d)\n", state);
  }
}

/**
 * @brief Registra dados no cartão SD (datalogger)
 */
void logToSD() {
  static File logFile;
  static bool firstRun = true;
  
  // Nome do arquivo com timestamp
  String filename = "/flight_log.csv";
  
  if (firstRun) {
    // Cria arquivo com cabeçalho
    logFile = SD.open(filename, FILE_WRITE);
    if (logFile) {
      logFile.println("timestamp,latitude,longitude,altitude,velocity,temperature,humidity,pressure,gas_resistance,gps_fix");
      logFile.close();
      DBG_PRINT(2, "[SD] Arquivo de log criado: %s\n", filename.c_str());
    }
    firstRun = false;
  }
  
  // Adiciona dados
  logFile = SD.open(filename, FILE_APPEND);
  if (logFile) {
    String logLine = "";
    logLine += String(sensorData.timestamp) + ",";
    logLine += String(sensorData.latitude, 6) + ",";
    logLine += String(sensorData.longitude, 6) + ",";
    logLine += String(sensorData.altitude, 1) + ",";
    logLine += String(sensorData.velocity, 1) + ",";
    logLine += String(sensorData.temperature, 1) + ",";
    logLine += String(sensorData.humidity, 1) + ",";
    logLine += String(sensorData.pressure, 1) + ",";
    logLine += String(sensorData.gasResistance, 0) + ",";
    logLine += String(sensorData.gpsFix ? "1" : "0");
    
    logFile.println(logLine);
    logFile.close();
    
    DBG_PRINT(3, "[SD] Dados registrados\n");
  } else {
    DBG_PRINT(1, "[SD] Erro ao abrir arquivo de log\n");
  }
}

/**
 * @brief Atualiza o display LCD
 */
void updateDisplay() {
  // TODO: Implementar atualização do display
  // Mostrar: altitude, velocidade, temperatura, GPS fix, bateria, etc.
}

// ==================== Setup e Loop ====================

/**
 * @brief Inicialização do sistema
 */
void setup() {
  // Inicializa Serial para debug
  Serial.begin(115200);
  delay(1000);
  
  DBG_PRINT(2, "\n========================================\n");
  DBG_PRINT(2, "Tiny Water Station Mobile - v2.1.0\n");
  DBG_PRINT(2, "========================================\n");
  
  // Inicializa interfaces
  initI2C();
  initGPS();
  
  // Inicializa SPI (será usado por LoRa, SD, Display)
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  
  // Inicializa periféricos
  initBME688();
  initLoRa();
  initSD();
  initDisplay();
  
  // Verifica sensores I2C
  checkI2CDevice(0x76, "BMP581");  // Pressão alta precisão
  checkI2CDevice(0x40, "BMA400");  // Acelerômetro
  checkI2CDevice(0x12, "BMM350");  // Magnetômetro
  checkI2CDevice(0x53, "LTR-390"); // UV/Luz
  checkI2CDevice(0x36, "MAX17048"); // Fuel gauge
  
  DBG_PRINT(2, "\nInicialização concluída!\n");
  DBG_PRINT(2, "========================================\n\n");
}

/**
 * @brief Loop principal
 */
void loop() {
  static unsigned long lastSensorRead = 0;
  static unsigned long lastLoRaTx = 0;
  static unsigned long lastDisplayUpdate = 0;
  
  const unsigned long sensorInterval = 100;    // 10 Hz
  const unsigned long loRaInterval = 1000;     // 1 Hz
  const unsigned long displayInterval = 200;   // 5 Hz
  
  unsigned long currentMillis = millis();
  
  // Lê sensores
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    updateSensors();
  }
  
  // Transmite via LoRa
  if (currentMillis - lastLoRaTx >= loRaInterval) {
    lastLoRaTx = currentMillis;
    transmitLoRa();
  }
  
  // Atualiza display
  if (currentMillis - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = currentMillis;
    updateDisplay();
  }
  
  // Registra dados no SD a cada segundo
  static unsigned long lastSDLog = 0;
  if (currentMillis - lastSDLog >= 1000) {
    lastSDLog = currentMillis;
    logToSD();
  }
}

