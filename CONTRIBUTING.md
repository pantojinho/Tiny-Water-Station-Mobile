# Contributing to Tiny Water Station Mobile

Thank you for your interest in contributing to the **Tiny Water Station Mobile**! This document provides guidelines for contributions in two main areas: **PCB/Hardware** and **Software**.

---

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Contribute](#how-to-contribute)
- [PCB/Hardware Contributions](#pcbhardware-contributions)
- [Software Contributions](#software-contributions)
- [Pull Request Process](#pull-request-process)
- [Coding Standards](#coding-standards)
- [Reporting Bugs](#reporting-bugs)
- [Requesting Features](#requesting-features)

---

## 🤝 Code of Conduct

By participating in this project, you agree to maintain a respectful and inclusive environment. Be constructive in your criticism and welcoming to new contributors.

---

## 🚀 How to Contribute

### Initial Steps

1. **Fork** the repository
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/your-username/Tiny-Water-Station-Mobile.git
   cd Tiny-Water-Station-Mobile
   ```
3. **Create a branch** for your contribution:
   ```bash
   git checkout -b feature/your-feature
   ```
4. **Make your changes** and test
5. **Commit** your changes:
   ```bash
   git commit -m "Add new feature XYZ"
   ```
6. **Push** to your fork:
   ```bash
   git push origin feature/your-feature
   ```
7. **Open a Pull Request** describing your changes

---

## 🔌 PCB/Hardware Contributions

Hardware contributions should be directed to the **`PCB/`** folder.

### Suggested File Structure

```
PCB/
├── schematics/           # Schematics
│   ├── main_schematic.pdf
│   └── power_circuit.pdf
├── layouts/              # PCB Layouts
│   ├── esp32_main_v1.1.brd
│   └── gerber_files/
├── bom/                  # Bill of Materials
│   ├── bom.csv
│   └── components_list.xlsx
├── 3d-models/            # 3D Models for printing
│   ├── case/
│   └── mounting_brackets/
├── documentation/        # Technical documentation
│   ├── sensor_placement.md
│   └── assembly_guide.md
└── revisions/            # Version control
    ├── v1.0/
    └── v1.1/
```

### Hardware Guidelines

1. **Versioning:** Always maintain version control on PCB files (e.g., v1.1, v1.2)
2. **Documentation:** Include design notes, especially for:
   - BMM350 positioning (distance from magnetic sources)
   - Openings for BME688 sensor
   - GPS connector routing
3. **Updated BOM:** Keep the Bill of Materials updated with:
   - Part Numbers
   - Recommended suppliers
   - Compatible alternatives
4. **Gerber Files:** Include complete Gerber files for manufacturing
5. **Test Points:** Include test points for:
   - Voltage verification
   - I2C bus testing
   - UART/SPI signal verification
6. **Design Considerations:**
   - Be specific about distances and orientations
   - Include manufacturing constraints (trace width, clearance)
   - Document any necessary firmware modifications

### Types of Hardware Contributions

- ✅ Schematic error fixes
- ✅ Layout optimizations (noise reduction, better routing)
- ✅ Addition of new features (e.g., new sensors via expansion header)
- ✅ 3D case design improvements
- ✅ BOM updates with alternative components
- ✅ Documentation of known issues

---

## 💻 Software Contributions

Software contributions should be directed to the **`software/`** folder.

### Suggested File Structure

```
software/
├── firmware/             # Main firmware (ESP32)
│   ├── src/
│   │   ├── main.cpp
│   │   ├── sensors/
│   │   │   ├── bme688.cpp
│   │   │   ├── bmp581.cpp
│   │   │   ├── bma400.cpp
│   │   │   ├── bmm350.cpp
│   │   │   ├── ltr390.cpp
│   │   │   └── max17048.cpp
│   │   ├── comms/
│   │   │   ├── lora.cpp
│   │   │   ├── gps.cpp
│   │   │   └── sd_card.cpp
│   │   ├── display/
│   │   │   └── lcd_controller.cpp
│   │   └── utils/
│   │       └── data_logger.cpp
│   ├── include/
│   ├── platformio.ini
│   └── CMakeLists.txt
├── libraries/            # Custom libraries
├── tools/                # Utility scripts
│   ├── flash_tool.py
│   └── data_parser.py
├── tests/                # Unit tests
└── documentation/        # Technical documentation
    ├── api_reference.md
    ├── sensor_drivers.md
    └── configuration.md
```

### Software Guidelines

1. **Language:** The main firmware should be developed in C/C++
2. **Platform:** Use **PlatformIO** as build manager
3. **Code Style:**
   - Follow ESP-IDF code standards
   - Use descriptive English names for variables and functions
   - Use English comments to explain complex logic
   - Consistent indentation (recommended: 2 spaces)
4. **Sensor Drivers:**
   - Create dedicated drivers for each sensor in `sensors/` folder
   - Keep initialization and configuration in separate functions
   - Implement appropriate error handling
5. **Datalogger:**
   - Standardized CSV format for flight logs
   - ISO 8601 timestamp format
   - SD card file rotation
6. **LoRa:**
   - Use RadioLib or SX126x-Arduino library
   - Implement CRC in packets
   - Handle packet loss
7. **Comments and Documentation:**
   - Document public APIs in header files
   - Include usage examples
   - Explain critical calculations (especially altitude/pressure)

### Required Sensor Drivers

| Sensor | Base Library | Notes |
|--------|----------------|-------------|
| BME688 | Bosch BSEC 2.x | Update to BSEC2 |
| BMP581 | Bosch BMP5-Sensor-API | |
| BMA400 | Bosch BMA400-API | Low-Power Mode |
| BMM350 | Bosch BMM350-API | High precision |
| LTR-390 | Adafruit LTR3XX | |
| MAX17048 | Adafruit MAX1704X | |

### Types of Software Contributions

- ✅ Bug fixes
- ✅ Addition of new sensor drivers
- ✅ Performance optimizations
- ✅ Power management improvements
- ✅ Addition of new operation modes
- ✅ Implementation of new LoRa features
- ✅ LCD display improvements
- ✅ Unit tests
- ✅ Code documentation

---

## 🔄 Pull Request Process

### Before Opening a PR

1. **Test your changes:**
   - Hardware: Verify PCB can be manufactured without errors
   - Software: Compile and test on real hardware when possible

2. **Document your changes:**
   - Update README if necessary
   - Add notes in relevant documentation
   - Include comments in code for complex changes

3. **Sync with upstream:**
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

### PR Template

```markdown
## Description
Brief description of implemented changes.

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Hardware change
- [ ] Software change
- [ ] Documentation

## Motivation
Why is this change necessary? What problem does it solve?

## How to Test
Steps to test the changes:

1. ...
2. ...

## Checklist
- [ ] My code follows style standards
- [ ] I have tested (manual/automated)
- [ ] I have updated documentation
- [ ] No merge conflicts
- [ ] I have reviewed my own changes

## Screenshots/Logs
If applicable, include screenshots or logs.
```

---

## 📝 Coding Standards

### C/C++ (ESP32 Firmware)

```cpp
// ✅ Good
float readAltitude() {
    float pressure = readPressure();
    return 44330 * (1 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

// ❌ Bad
float getA(){float p=getP();return 44330*(1-pow(p/101325,0.1903));}
```

- Use appropriate types (`uint8_t`, `float`, etc.)
- Functions should have descriptive names
- Constants in UPPER_CASE
- Variables in snake_case
- Classes in CamelCase

### Documentation

```cpp
/**
 * @brief Initialize BMM350 sensor
 * 
 * @param rate Desired sampling rate (BMM350_DATA_RATE_100HZ, etc.)
 * @return true if initialization successful
 * @return false if I2C communication failed
 */
bool initBMM350(uint8_t rate);
```

---

## 🐛 Reporting Bugs

When reporting a bug, include:

1. **Clear description** of the problem
2. **Steps to reproduce**
3. **Expected vs. actual behavior**
4. **Environment information:**
   - Hardware version (v1.0, v1.1)
   - Firmware version
   - PlatformIO/ESP-IDF version
5. **Logs or screenshots** if applicable

### Bug Report Template

```markdown
## Bug Description
[What happened?]

## Steps to Reproduce
1. [...]
2. [...]
3. [...]

## Expected Behavior
[What should happen?]

## Environment
- Hardware version: [e.g., v1.1]
- Firmware version: [e.g., 2.1.0]
- PlatformIO: [e.g., 6.1.0]

## Logs/Output
[Paste relevant logs here]
```

---

## ✨ Requesting Features

Before requesting a new feature:

1. Check if a similar issue already exists
2. Clearly describe the use case
3. Explain the value it adds to the project
4. If possible, consider contributing with the implementation

### Feature Request Template

```markdown
## Feature Title
[Brief description]

## Detailed Description
[Explain the desired feature]

## Use Case
[How will it be used? Why is it useful?]

## Alternatives Considered
[What other solutions exist? Why is this better?]

## Suggested Implementation
[If you have ideas on how to implement]
```

---

## 📧 Contact

If you have questions about how to contribute:

- Open an **Issue** for public discussions
- Contact maintainers for specific questions

---

## 📜 License

By contributing, you agree that your contributions will be licensed under the project's **MIT** license.

---

**Thank you for contributing to Tiny Water Station Mobile! 🚀**
