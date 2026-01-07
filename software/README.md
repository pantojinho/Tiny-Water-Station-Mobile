# Software - Tiny Water Station Mobile

This folder contains all source code, libraries, tools, and documentation related to software development for the ESP32-S3 of Tiny Water Station Mobile.

---

## 📁 Directory Structure

```
software/
├── firmware/             # Main firmware (ESP32-S3)
│   ├── src/              # Source files (.cpp, .c, .ino)
│   │   ├── main.cpp      # Main file
│   │   ├── sensors/      # Sensor drivers
│   │   ├── comms/        # Communication (LoRa, GPS, SD)
│   │   ├── display/      # LCD control
│   │   └── utils/        # Utility functions
│   ├── include/          # Headers (.h)
│   ├── platformio.ini    # PlatformIO configuration
│   └── CMakeLists.txt    # Build system (ESP-IDF)
├── libraries/            # Custom libraries
├── tools/                # Scripts and utilities
├── tests/                # Unit and integration tests
└── documentation/        # Technical documentation and APIs
```

---

## 🚀 Getting Started

### Requirements

- **PlatformIO** (CLI or VS Code extension) - Recommended
- **ESP-IDF** (optional, for native builds)
- **Python 3.8+** (for script tools)
- **Git** (for version control)

### Installation

#### Using PlatformIO (Recommended)

1. Install VS Code: https://code.visualstudio.com/
2. Install PlatformIO extension
3. Clone the repository:
   ```bash
   git clone https://github.com/your-username/Tiny-Water-Station-Mobile.git
   cd Tiny-Water-Station-Mobile/software/firmware
   ```
4. Open in VS Code
5. PlatformIO will automatically detect `platformio.ini`

#### Using ESP-IDF (Alternative)

```bash
# Clone the repository
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh

# Clone and compile the project
git clone https://github.com/your-username/Tiny-Water-Station-Mobile.git
cd Tiny-Water-Station-Mobile/software/firmware
idf.py build
```

### Compilation and Upload

```bash
# Compile
pio run

# Upload to ESP32-S3
pio run --target upload

# Serial monitor
pio device monitor

# All together
pio run --target upload --target monitor
```

---

## 📦 Hardware Components (BOM)

### Required Hardware Components

The following hardware components are required for software development and testing:

| Component | Description | Quantity | Notes |
|-----------|-------------|-----------|--------|
| **ESP32-S3-WROOM-1-N16R8** | Main microcontroller module | 1 | 16MB Flash, 8MB PSRAM |
| **BME688** | Environmental sensor (Temp, Humidity, Gas) | 1 | I2C address: 0x77 |
| **BMP581** | High-precision pressure sensor | 1 | I2C address: 0x76 |
| **BMA400** | Low-power accelerometer | 1 | I2C address: 0x40 |
| **BMM350** | High-precision magnetometer | 1 | I2C address: 0x12 |
| **LTR-390** | UV and ambient light sensor | 1 | I2C address: 0x53 |
| **MAX17048** | Battery fuel gauge | 1 | I2C address: 0x36 |
| **SX1262/CC68** | LoRa transceiver module | 1 | 915 MHz, SPI interface |
| **GPS Module** | GPS receiver module | 1 | NMEA protocol, UART interface |
| **IPS LCD Display 1.14"** | Color display | 1 | 135x240 pixels, SPI interface |
| **MicroSD Card** | Data storage | 1 | Minimum 4GB, FAT32 formatted |
| **Li-Po Battery** | Power source | 1 | 3.7V, 1000mAh+ recommended |
| **USB-C Cable** | Programming and charging | 1 | For ESP32 upload and battery charging |
| **JST-PH Connector** | Battery connector | 1 | 2-pin 2.0mm pitch |

### Development Tools

| Tool | Description | Purpose |
|-------|-------------|---------|
| **ESP32-S3 DevKit** | Development board | Initial testing before PCB |
| **Logic Analyzer** | Digital signal debugging | I2C, SPI, UART protocol analysis |
| **Multimeter** | Electrical measurements | Voltage, current, continuity testing |
| **Oscilloscope** | Signal visualization | Optional, for advanced debugging |
| **USB-UART Adapter** | Serial communication | GPS module testing (optional) |
| **LoRa Gateway** | LoRa receiver | Testing long-range transmission |

### Optional Accessories

| Component | Description | Purpose |
|-----------|-------------|---------|
| **Antenna U.FL (LoRa)** | LoRa antenna | 915MHz, dipole or helical |
| **Antenna U.FL (GPS)** | GPS antenna | Active or passive, 1575.42MHz |
| **Push Buttons** | Manual controls | Testing emergency mode, configuration |
| **LEDs** | Status indicators | Visual feedback (optional) |
| **3D Printed Case** | Enclosure | Protects electronics during testing |

### Total Component Count

- **Sensors (I2C):** 6 components
- **Communication Modules:** 2 components (LoRa + GPS)
- **Interfaces:** 2 components (Display + SD Card)
- **Power System:** 2 components (Battery + Charger)
- **Total Unique Components:** ~12 items

### Notes

1. All I2C sensors share the same bus (SDA: GPIO 47, SCL: GPIO 48)
2. SD Card and Display share the same SPI bus (different CS pins)
3. LoRa module uses dedicated SPI interface
4. GPS uses UART (GPIO 17/18)
5. Ensure all sensors are properly powered (3.3V) before testing software

---

## 📡 Sensors and Drivers

### I2C Architecture

All sensors are on the I2C bus:
- **SDA:** GPIO 47
- **SCL:** GPIO 48

### Implemented Drivers

| Sensor | Driver | Base Library | Status |
|--------|--------|-----------------|--------|
| BME688 | `bme688.cpp` | Bosch BSEC 2.x | ✅ Implemented |
| BMP581 | `bmp581.cpp` | Bosch BMP5-Sensor-API | ✅ Implemented |
| BMA400 | `bma400.cpp` | Bosch BMA400-API | ✅ Implemented |
| BMM350 | `bmm350.cpp` | Bosch BMM350-API | ✅ Implemented |
| LTR-390 | `ltr390.cpp` | Adafruit LTR3XX | ✅ Implemented |
| MAX17048 | `max17048.cpp` | Adafruit MAX1704X | ✅ Implemented |

### Initialization Example

```cpp
#include "sensors/bme688.h"

void setup() {
    Serial.begin(115200);
    
    // Initialize BME688
    if (bme688_init()) {
        Serial.println("BME688 initialized successfully!");
    } else {
        Serial.println("Error initializing BME688");
    }
}
```

---

## 📡 Communication

### LoRa (SX126x/CC68)

- **Interface:** SPI
- **Pins:**
  - MOSI: GPIO 11
  - MISO: GPIO 13
  - SCK: GPIO 12
  - CS: GPIO 10
  - DIO1: GPIO 14
- **Frequency:** 915 MHz (South America)
- **Library:** RadioLib

### GPS

- **Interface:** UART
- **Pins:**
  - RX (GPIO 17): Receives from GPS
  - TX (GPIO 18): Sends to GPS
- **Baud rate:** 9600 (configurable)
- **Protocol:** NMEA

### SD Card

- **Interface:** SPI (shared)
- **CS Pin:** GPIO 21
- **Format:** FAT32
- **Function:** Flight datalogger (CSV)

---

## 🖥️ LCD Display

### Specifications

- **Model:** IPS LCD 1.14"
- **Resolution:** 135x240 pixels
- **Interface:** SPI (shared)
- **Pins:**
  - CS: GPIO 9
  - DC: GPIO 15
- **Library:** TFT_eSPI or Adafruit ST7735

### Interface Layout

```
┌─────────────────────────┐
│ Alt: 1245m  Vel: 12.3m/s│  Top bar
├─────────────────────────┤
│ Temp: 25.4°C           │
│ Umid: 68%              │
│ Pres: 1013.2 hPa       │  Sensor data
│ UV: 3.2 Lux: 45000     │
├─────────────────────────┤
│ Bat: 85%  GPS: Fix     │  System status
│ LoRa: TX 2.4k/s        │
└─────────────────────────┘
```

---

## 📊 Datalogger

### Log Format (CSV)

The system records data in CSV format with timestamp:

```csv
timestamp,latitude,longitude,altitude,velocity,temperature,humidity,pressure,gas_resistance,accel_x,accel_y,accel_z,mag_x,mag_y,mag_z,heading,uv_index,lux,battery_percent,lo_rssi
2024-01-06T14:30:00.000Z,-23.5505,-46.6333,1245.2,12.3,25.4,68,1013.2,45000,0.02,0.01,9.81,-23.5,12.3,-45.2,145.3,3.2,45000,85,-65
```

### File Rotation

- **Maximum file size:** 10 MB
- **File name:** `flight_log_YYYYMMDD_HHMMSS.csv`
- **Total logs:** Up to 50 files (after that, the oldest is deleted)

---

## 🔧 Configuration

### platformio.ini

```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; Memory settings
board_build.arduino.memory_type = qio_opi
board_build.partitions = huge_app.csv

; Serial settings
monitor_speed = 115200
monitor_filters = esp32_exception_decoder

; Libraries
lib_deps = 
    adafruit/Adafruit Unified Sensor
    adafruit/Adafruit BME680 Library
    RadioLib
    TFT_eSPI
    SPIFFS

; Compilation flags
build_flags = 
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
```

---

## 🧪 Tests

### Running Tests

```bash
# Test compilation
pio run --target test

# Run all tests
pio test

# Specific test
pio test --target test_bme688
```

### Available Tests

| Test | Description |
|-------|-----------|
| `test_i2c` | Tests communication with all I2C sensors |
| `test_lora` | Tests LoRa transmission and reception |
| `test_gps` | Tests NMEA parsing |
| `test_sd` | Tests SD card writing and reading |
| `test_display` | Tests LCD rendering |

---

## 📚 Documentation

### API Documentation

See the `documentation/` folder for:

- **API Reference:** Complete documentation of public functions
- **Sensor Drivers:** Detailed guide for each sensor
- **Configuration:** Configurable parameters
- **Integration Guide:** Guide for integrating new modules

### Inline Documentation

All public functions have Doxygen documentation:

```cpp
/**
 * @brief Reads current altitude from BMP581 sensor
 * 
 * @return float Altitude in meters above sea level
 * 
 * @note Requires BMP581 to be initialized
 * @see init_bmp581()
 */
float read_altitude();
```

---

## 🎯 Operation Modes

### Normal Mode

Standard operation for monitoring:
- Sensors read at 10 Hz
- LoRa transmitting at 1 Hz
- Display updated at 5 Hz
- Continuous SD logging

### Power Saving Mode

For long missions:
- Sensors read at 1 Hz
- LoRa transmitting at 0.1 Hz (1 msg/10s)
- Display off or updated at 1 Hz
- ESP32 in deep sleep between readings

### Emergency Mode

Triggered by:
- Battery < 10%
- Temperature > 60°C
- Impact detection (BMA400)

Actions:
- Stop LoRa transmissions
- Log only critical data
- LEDs flashing red

---

## 🐛 Debugging

### Debug Logs

```cpp
// Available log levels
ESP_LOGE("TAG", "Error: %s", message);   // Error
ESP_LOGW("TAG", "Warning: %s", message);  // Warning
ESP_LOGI("TAG", "Info: %s", message);   // Info
ESP_LOGD("TAG", "Debug: %s", message);  // Debug
ESP_LOGV("TAG", "Verbose: %s", message);// Verbose
```

### Serial Monitoring

```bash
# Open serial monitor with filter
pio device monitor --filter time,esp32_exception_decoder

# Export logs to file
pio device monitor > debug.log
```

---

## 🤝 Contributing with Software

To contribute:

1. **Create a branch:**
   ```bash
   git checkout -b software/new-feature
   ```

2. **Make your modifications:**
   - Follow code standards (C/C++)
   - Add tests for new features
   - Update documentation

3. **Test:**
   ```bash
   pio test
   pio run
   ```

4. **Commit and Push:**
   ```bash
   git add .
   git commit -m "Add feature X"
   git push origin software/new-feature
   ```

5. **Open a Pull Request** following the template in `../CONTRIBUTING.md`

---

## 📚 Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_en.pdf)
- [Bosch Sensor API Documentation](https://github.com/BoschSensortec/BMI160_SensorAPI)
- [Contribution Documentation](../CONTRIBUTING.md)
- [Reference Pinout](../README.md#6-firmware-reference-pinout)

---

**Last Update:** 2026
**Firmware Version:** 2.1.0
**Framework:** Arduino / ESP-IDF 5.x
