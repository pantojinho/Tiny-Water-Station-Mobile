# PCB / Hardware - Tiny Water Station Mobile

This folder contains all files related to hardware design, schematics, PCB layouts, and technical documentation for manufacturing.

---

## 📁 Directory Structure

```
PCB/
├── schematics/           # Electronic schematics
├── layouts/              # PCB Layouts (EDA files)
├── bom/                  # Bill of Materials (Component list)
├── 3d-models/            # 3D Models (case, brackets, etc.)
├── documentation/        # Technical documentation
└── revisions/            # Version control per hardware
```

---

## 📋 Current Content

### Version 1.1 (Current)

This version uses the following main components:

- **MCU:** ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
- **I2C Sensors:**
  - BME688 - Temperature, Humidity, Gas (VOC/VSC)
  - BMP581 - Pressure and Altitude Pro
  - BMA400 - Low-Power Accelerometer
  - BMM350 - High-Precision Magnetometer
  - LTR-390 - UV Index and Luxmetry
  - MAX17048 - Battery Fuel Gauge

- **Communication:**
  - LoRa CC68 915MHz
  - GPS (external 6-pin female connector, 2.54mm pitch)

- **Interfaces:**
  - IPS LCD Display 1.14" (SPI)
  - MicroSD Card (SPI)
  - Expansion Header 1x08

---

## 🛠️ Recommended Tools

### PCB Design

- **KiCad (7.x or higher):** Free and open-source software
- **Altium Designer:** Professional tool (requires license)
- **EasyEDA:** Web-based alternative

### 3D Modeling

- **Fusion 360:** Professional CAD (free for educational use)
- **FreeCAD:** Open-source alternative
- **OpenSCAD:** Script-based for parametric models

### BOM and Documentation

- **Excel/Google Sheets:** For component lists
- **Markdown:** For technical documentation

---

## 📌 Important Design Notes

### Sensor Placement

1. **BMM350 (Magnetometer):**
   - Position as far as possible from magnetic sources
   - Avoid DC-DC converter inductors
   - Keep distance from speakers (if any)

2. **BME688 (Gas Sensor):**
   - The 3D case needs an opening for ventilation
   - Air flow required for accurate measurements
   - Avoid hermetic encapsulation

3. **Antennas:**
   - U.FL connectors for GPS and LoRa
   - Keep free area for radiation
   - Avoid metal traces under antennas

### Manufacturing Requirements

- **Minimum trace width:** 6 mil (0.15mm) for signals
- **Minimum clearance:** 6 mil (0.15mm)
- **Mounting vias:** Use tented vias
- **Solder mask:** Cover critical areas
- **Silkscreen:** Clear markers for SMD components

### Required Gerber Files

When sending for manufacturing, include:
- `.GTL` - Top copper layer
- `.GBL` - Bottom copper layer
- `.GTO` - Top silkscreen
- `.GBO` - Bottom silkscreen
- `.GBS` - Bottom solder mask
- `.GTS` - Top solder mask
- `.GBO` - Board outline
- `.GTP` - Top paste
- `.GBP` - Bottom paste

---

## 🔄 Version Control

Each hardware version should have its own folder in `revisions/` section:

```
revisions/
├── v1.0/
│   ├── schematics/
│   ├── layouts/
│   └── bom_v1.0.csv
└── v1.1/
    ├── schematics/
    ├── layouts/
    └── bom_v1.1.csv
```

### Versioning Convention

- **Major:** Changes that make versions incompatible
- **Minor:** Backward-compatible additions
- **Patch:** Bug fixes

---

## 📦 Bill of Materials (BOM)

### Recommended Format

```csv
Ref,Value,Footprint,Quantity,Manufacturer,Part Number,Supplier,Supplier Part Number,Notes
U1,ESP32-S3-WROOM-1,N16R8,1,Espressif,ESP32-S3-WROOM-1-N16R8,Digikey,2877-ESP32-S3-WROOM-1-N16R8-CT,
U2,BME688,LGA-8,1,Bosch,BME688,Digikey,828-1083-1-ND,
...
```

### Required Fields

- **Ref:** Reference on PCB (U1, R1, C1, etc.)
- **Value:** Component value (10k, 0.1uF, etc.)
- **Footprint:** Footprint used in layout
- **Quantity:** Required quantity
- **Part Number:** Manufacturer part number
- **Supplier Part Number:** Supplier part number

---

## 🎯 3D Case Design

### Minimum Requirements

1. **Required Openings:**
   - BME688 sensor (ventilation)
   - LCD Display (visible)
   - USB-C connector (charging)
   - LED indicators
   - Antennas (GPS/LoRa) - transparent or external area

2. **Battery Support:**
   - Fixed compartment for Li-Po battery
   - Avoid vibration that can disconnect JST
   - Easy access for replacement

3. **Assembly:**
   - Mounting holes for PCB
   - Appropriate spacers/standoffs
   - Consider ESP32 thermal dissipation

### 3D Printing Files

Recommended to provide:
- **STL/OBJ:** For direct printing
- **STEP:** For CAD modification
- **Printing Instructions:**
  - Recommended material (PLA/PETG/ABS)
  - Infill %
  - Temperature
  - Layers

---

## 🔧 Assembly and Soldering

### Suggested Assembly Order

1. Passive components (resistors, capacitors)
2. Connectors and headers
3. Integrated circuits
4. Mechanical components (buttons, LEDs)
5. Modules via headers (GPS, LoRa)

### Soldering Notes

- Use flux solder for SMD components
- Recommended temperature: 350-370°C
- Maximum contact time: 3 seconds per pin
- Check continuity after assembly

---

## 📞 Contributing with Hardware

To contribute with new designs or improvements:

1. Create a branch following the pattern: `hardware/name-of-change`
2. Make your modifications
3. Test the schematic (DRC/ERC)
4. Update BOM if necessary
5. Document the changes
6. Open a Pull Request with detailed description

Consult the file `../CONTRIBUTING.md` for more details about the contribution process.

---

## 📚 Additional Resources

- [Component Datasheets](../README.md#3-sensor-architecture-i2c-bus)
- [Complete Pinout](../README.md#6-firmware-reference-pinout)
- [Contribution Documentation](../CONTRIBUTING.md)
- [China Manufacturing Guidelines](../README.md#7-notes-for-3d-design-and-pcb-china)

---

**Last Update:** 2026
**Version:** 1.1
