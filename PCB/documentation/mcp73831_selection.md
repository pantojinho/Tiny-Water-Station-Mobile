# MCP73831 Battery Charger Selection Guide

## Overview

The **MCP73831** is a linear charge management controller for single-cell lithium-ion or lithium-polymer batteries. This document explains which variant to use for the Tiny Water Station Mobile project.

---

## Available Variants on LCSC

Based on [LCSC Electronics](https://www.lcsc.com/), the following MCP73831 variants are available:

### 1. MCP73831-2DCI/MC (Recommended)
- **Voltage Regulation:** 4.2V (standard for 3.7V Li-Po batteries)
- **Package:** DFN-8-EP (2x3mm)
- **LCSC Part Number:** C635037
- **Link:** [MCP73831-2DCI/MC on LCSC](https://lcsc.com/product-detail/Battery-Management-ICs_Microchip-Tech-MCP73831-2DCI-MC_C635037.html)
- **Advantages:**
  - Standard 4.2V charge voltage (perfect for 3.7V Li-Po)
  - DFN package is easier to solder than TDFN
  - Good thermal performance
  - Suitable for space-constrained designs

### 2. MCP73831-2ACI/MC (Alternative)
- **Voltage Regulation:** 4.2V
- **Package:** TDFN-8-EP (2x3mm)
- **LCSC Part Number:** C150772
- **Link:** [MCP73831-2ACI/MC on LCSC](https://lcsc.com/product-detail/_MICROCHIP_MCP73831-2ACI-MC_MCP73831-2ACI-MC_C150772.html)
- **Note:** Similar to 2DCI but with slightly different thermal characteristics

### 3. MCP73831-5ACI/MC (Not Recommended)
- **Voltage Regulation:** 4.5V
- **Package:** DFN-8-EP (2x3mm)
- **LCSC Part Number:** C635040
- **Note:** 4.5V is too high for standard 3.7V Li-Po batteries (would damage them)

---

## Selection for Tiny Water Station Mobile

### Recommended Part: **MCP73831-2DCI/MC**

**Why this variant?**

1. **Correct Voltage:** 4.2V is the standard charge termination voltage for 3.7V Li-Po batteries
2. **Package:** DFN-8-EP is suitable for SMD assembly and has good thermal properties
3. **Availability:** Available on LCSC with part number C635037
4. **Compatibility:** Works perfectly with the DW01A battery protection IC

### Specifications

- **Input Voltage:** 4.5V to 6.0V (USB-C provides 5V)
- **Charge Current:** Programmable via external resistor (typically 500mA to 1000mA)
- **Charge Termination:** 4.2V ± 0.05V
- **Package:** DFN-8-EP (2mm x 3mm)
- **Pin Count:** 8 pins with exposed thermal pad

---

## PCB Design Considerations

### Footprint Requirements

- **Package:** DFN-8-EP (2mm x 3mm)
- **Pitch:** 0.5mm (fine pitch - requires careful routing)
- **Thermal Pad:** Exposed pad on bottom (must be connected to GND for heat dissipation)
- **Solder Paste:** Use stencil with appropriate aperture for fine-pitch components

### Layout Guidelines

1. **Thermal Pad:** Connect the exposed pad to a large ground plane for heat dissipation
2. **Input Capacitor:** Place 10µF ceramic capacitor close to VIN pin
3. **Output Capacitor:** Place 10µF ceramic capacitor close to VOUT pin
4. **Charge Current Resistor:** Place programming resistor (R_PROG) close to PROG pin
5. **Thermal Vias:** Add thermal vias under the thermal pad if possible

### Typical Application Circuit

```
USB-C (5V) ──┬── VIN (Pin 1)
             │
             ├── 10µF ── GND
             │
             └── MCP73831-2DCI/MC
                    │
                    ├── VOUT (Pin 8) ── Li-Po Battery (+)
                    │
                    ├── PROG (Pin 2) ── R_PROG ── GND
                    │
                    └── STAT (Pin 3) ── LED (optional status indicator)
```

### Charge Current Programming

The charge current is set by the resistor connected to the PROG pin:

| R_PROG (kΩ) | Charge Current (mA) |
|-------------|---------------------|
| 2.0         | 1000                |
| 2.5         | 800                 |
| 3.0         | 650                 |
| 4.0         | 500                 |
| 5.0         | 400                 |

**Recommended:** 2.5kΩ for 800mA charge current (safe for 1000mAh battery)

---

## Ordering Information

### LCSC Ordering Details

- **Manufacturer:** Microchip Technology
- **Part Number:** MCP73831-2DCI/MC
- **LCSC Part Number:** C635037
- **Package:** DFN-8-EP (2x3mm)
- **Minimum Order Quantity:** Check LCSC website
- **Stock Status:** Verify availability before ordering

### Alternative Suppliers

If LCSC is out of stock, consider:
- **Digikey:** MCP73831T-2DCI/OT (similar part, different package)
- **Mouser:** Check for MCP73831 variants
- **Arrow:** Alternative distributor

---

## Testing and Validation

### Before Assembly

1. Verify package dimensions match PCB footprint
2. Check thermal pad connection to ground plane
3. Verify input/output capacitor placement
4. Confirm charge current resistor value

### After Assembly

1. **Voltage Test:** Measure VOUT should be 4.2V when battery is fully charged
2. **Current Test:** Measure charge current should match programmed value
3. **Thermal Test:** Monitor temperature during charging (should stay below 85°C)
4. **Status LED:** Verify STAT pin indicates charge status correctly

---

## Troubleshooting

### Common Issues

1. **No Charging:**
   - Check VIN voltage (should be 4.5V-6.0V)
   - Verify PROG resistor value
   - Check battery connection

2. **Overheating:**
   - Ensure thermal pad is properly soldered to ground
   - Check charge current (may be too high)
   - Verify adequate PCB copper area for heat dissipation

3. **Incorrect Charge Voltage:**
   - Verify correct variant (should be 2DCI for 4.2V)
   - Check for counterfeit parts

---

## References

- [LCSC Product Page - MCP73831-2DCI/MC](https://lcsc.com/product-detail/Battery-Management-ICs_Microchip-Tech-MCP73831-2DCI-MC_C635037.html)
- [Microchip MCP73831 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/20001984g.pdf)
- [LCSC Electronics Homepage](https://www.lcsc.com/)

---

**Last Updated:** 2026
**Selected Part:** MCP73831-2DCI/MC (LCSC: C635037)

