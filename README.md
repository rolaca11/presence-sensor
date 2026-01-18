# Presence Sensor

A compact Zigbee presence sensor based on the Texas Instruments CC2340R5 microcontroller.

## Features

- **CC2340R5** - Low-power Zigbee microcontroller
- **USB-C** - Power supply
- **Programming header** - 3-pin JST-SH connector for flashing firmware (compatible with Raspberry Pi Debug Probe)
- **3.3V LDO** - XC6206 voltage regulator
- **Two push buttons** - User input / reset
- **32.768 kHz & 48 MHz crystals** - For accurate timing and radio operation

## Project Structure

```
presence-sensor/
├── pcb/                    # KiCad 9 project files
│   ├── production/         # Manufacturing files (Gerbers, BOM, positions)
│   ├── footprints.pretty/  # Custom footprints
│   └── 3dmodels/           # 3D models for components
├── enclosure/              # 3D printable enclosure (STEP files)
└── LICENSE
```

## Manufacturing

Production files for PCB fabrication are located in `pcb/production/`:
- Gerber files (zipped)
- Bill of Materials (`bom.csv`)
- Pick and place positions (`positions.csv`)

The BOM includes LCSC part numbers for assembly at JLCPCB or similar services.

## Requirements

- [KiCad 9](https://www.kicad.org/) to view/edit the PCB design
- 3D printer or CNC for the enclosure (optional)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
