# Tiny Water Station Mobile - ESP32

**Project:** Atmospheric Telemetry and Aviation Mobile Unit

**Version:** 1.1 (Updated with BMM350)

**Processor:** ESP32-S3 (AI Enabled)

---

## 1. Overview

The **Tiny Water Station Mobile** is a high-performance development board (DEV Board) designed for weather balloon missions, critical environmental monitoring, and aviation assistance. The system combines industrial-grade precision sensors with long-range telemetry and edge artificial intelligence processing.

---

## 2. Core Specifications

The system is centered on the **ESP32-S3-WROOM-1-N16R8** module, chosen for:

* **Processing:** Dual-core with AI acceleration instructions (TensorFlow Lite Micro).
* **Memory:** 16MB Flash and 8MB PSRAM (essential for sensor buffers and LCD graphics).
* **Datalogger:** Integrated MicroSD slot for "Black Box" recording (flight logs in .csv).

---

## 3. Sensor Architecture (I2C Bus)

The board uses a shared I2C bus (GPIO 47 SDA / GPIO 48 SCL) with the following components:

| Sensor | Model | Function | Critical Advantage |
| --- | --- | --- | --- |
| **Environment** | Bosch BME688 | Gas (VOC/VSC), Temp and Humidity | AI for gas and odor detection |
| **Altimetry** | Bosch BMP581 | Pressure and Altitude Pro | 5cm precision (high-sensitivity vario) |
| **Inertial** | Bosch BMA400 | Low-Power Accelerometer | Impact detection and power saving |
| **Compass** | **Bosch BMM350** | **High-Precision Magnetometer** | **Heading correction and stable orientation** |
| **Light/UV** | Lite-On LTR-390 | UV Index and Luxmetry | Solar radiation monitoring |
| **Battery** | MAX17048 | Fuel Gauge (Monitoring) | Real battery percentage without charge loss |

---

## 4. External Module Integration (Off-Board)

To ensure flexibility and facilitate 3D assembly, the items below are connected via headers, not soldered directly to the main SMD board.

### 4.1. GPS Module (AliExpress)

The PCB will have a dedicated **6-pin female header** (2.54mm pitch) for the external GPS module. The GPS module from AliExpress comes with a 6-pin female connector.

* **PCB Pinout (6-pin female header):**
* **Pin 1:** VCC - 3.3V (AP2112K regulator output)
* **Pin 2:** GND - Common ground
* **Pin 3:** RXD (GPIO 17) - Connects to GPS module TX
* **Pin 4:** TXD (GPIO 18) - Connects to GPS module RX
* **Pin 5:** Reserved / NC
* **Pin 6:** Reserved / NC

*Note: Standard GPS modules typically use 4 pins (VCC, GND, TX, RX), but the AliExpress module uses a 6-pin connector. Pins 5 and 6 are reserved for future use or can be left unconnected.*



### 4.2. Power System and Battery

* **Type:** Li-Po 3.7V Battery (Recommended above 1000mAh for long missions).
* **Connector:** JST-PH 2.0mm 2-pin (Soldered on PCB).
* **Charging:** Integrated MCP73831 circuit for USB-C charging.
* **Safety:** DW01A protection circuit to prevent overcharge and deep discharge.

---

## 5. Interface and Expansion

### 5.1. Communication and Display

* **LoRa CC68 915MHz:** Long-distance data transmission (dedicated SPI).
* **IPS LCD Display 1.14":** Real-time visual feedback (shared SPI).
* **Antennas:** U.FL (IPEX) connectors for radio and GPS.

### 5.2. Expansion Header (Future-Proof)

A side connector (1x08) for future additions:

* `3.3V`, `GND`, `SDA`, `SCL`, `GPIO 1 (Analog)`, `GPIO 2`, `5V (VBUS)`.

---

## 6. Firmware Reference Pinout

| Feature | ESP32-S3 Pin | Note |
| --- | --- | --- |
| **I2C SDA / SCL** | GPIO 47 / 48 | Bus for all sensors |
| **UART TX / RX** | GPIO 18 / 17 | Communication with external GPS |
| **SPI MOSI/MISO/SCK** | GPIO 11 / 13 / 12 | Common bus (LoRa, LCD, SD) |
| **LoRa CS / DIO1** | GPIO 10 / 14 | Semtech radio control |
| **SD Card CS** | GPIO 21 | Datalogger selection |
| **LCD CS / DC** | GPIO 9 / 15 | IPS Display control |

---

## 7. Notes for 3D Design and PCB (China)

1. **Magnetometer (BMM350):** Position the BMM350 as far as possible from magnetic sources (such as speakers or DC-DC converter inductors) to avoid compass interference.
2. **Air Flow:** The BME688 sensor needs an opening in the 3D case to "breathe" external air, otherwise it will only measure the internal case temperature.
3. **GPS Module:** The 6-pin female connector should be positioned so that the external module cable does not pass over the ESP32-S3 antenna (WiFi/BT).
4. **Battery:** In the 3D design, provide a mechanical support for the Li-Po battery. Avoid leaving it loose, as in weather balloons vibration and wind can disconnect the JST.

---

## 8. Additional Documentation

For more technical details and contribution guidelines, see:

* **📦 [PCB/Hardware](PCB/README.md)** - Schematics, layouts, BOM and 3D design
* **💻 [Software](software/README.md)** - Firmware, drivers, tools and tests
* **🤝 [Contributing](CONTRIBUTING.md)** - Complete guide for contributors (PCB and Software)

---

## 9. License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for more details.

---

*Documentation generated to support hardware development and software integration.*
