# Presence Sensor

A compact Zigbee presence sensor based on the Texas Instruments CC2340R5 microcontroller and a UART-connected mmWave radar module. Detects human presence and reports occupancy over Zigbee, with configurable detection parameters.

## Features

- **CC2340R5** - Low-power Zigbee microcontroller
- **mmWave radar sensor** - UART-connected presence detection module with configurable range, sensitivity, and latency
- **Zigbee End Device** - Occupancy Sensing cluster with custom attributes for remote sensor configuration
- **USB-C** - Power supply
- **Programming header** - 3-pin JST-SH connector for flashing firmware (compatible with Raspberry Pi Debug Probe)
- **3.3V LDO** - XC6206 voltage regulator
- **Factory reset button** - Hold on boot to clear network credentials
- **32.768 kHz & 48 MHz crystals** - For accurate timing and radio operation

## Project Structure

```
presence-sensor/
├── pcb/                    # KiCad 9 project files
│   ├── production/         # Manufacturing files (Gerbers, BOM, positions)
│   ├── footprints.pretty/  # Custom footprints
│   └── 3dmodels/           # 3D models for components
├── firmware/               # Zigbee firmware (CCS project, TI SimpleLink SDK)
├── enclosure/              # 3D printable enclosure (STEP + STL files)
└── LICENSE
```

## Firmware

The firmware is a Code Composer Studio project built on the TI SimpleLink Low Power F3 SDK. It implements a Zigbee End Device that:

- Reads presence data from the mmWave sensor over UART (9600 baud)
- Reports occupancy via the standard **Occupancy Sensing** cluster
- Sends **On/Off** commands to bound devices when presence state changes
- Exposes custom Zigbee attributes (`0xE000`–`0xE008`) for configuring the sensor remotely:

| Attribute | ID | Type | Description |
|-|-|-|-|
| Range Min | `0xE000` | uint16 | Minimum detection range (cm) |
| Range Max | `0xE001` | uint16 | Maximum detection range (cm) |
| Trigger Range | `0xE002` | uint16 | Trigger detection range (cm) |
| Trigger Sensitivity | `0xE003` | uint8 | Trigger sensitivity (0–9) |
| Keep Sensitivity | `0xE004` | uint8 | Keep sensitivity (0–9) |
| Trigger Delay | `0xE005` | uint8 | Trigger delay (x10ms) |
| Keep Timeout | `0xE006` | uint16 | Keep timeout (x500ms) |
| IO Polarity | `0xE007` | uint8 | Output pin polarity |
| Micromotion | `0xE008` | bool | Micromotion detection enable |

### Building

1. Install [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) v12.7+
2. Install the [SimpleLink Low Power F3 SDK](https://www.ti.com/tool/SIMPLELINK-LOWPOWER-SDK) v9.14+
3. Import the `firmware/` directory as a CCS project
4. Build and flash via the 3-pin SWD header using a compatible debug probe

## Manufacturing

Production files for PCB fabrication are located in `pcb/production/`:
- Gerber files (zipped)
- Bill of Materials (`bom.csv`)
- Pick and place positions (`positions.csv`)

The BOM includes LCSC part numbers for assembly at JLCPCB or similar services.

## Requirements

- [KiCad 9](https://www.kicad.org/) to view/edit the PCB design
- [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) v12.7+ to build the firmware
- [SimpleLink Low Power F3 SDK](https://www.ti.com/tool/SIMPLELINK-LOWPOWER-SDK) v9.14+
- 3D printer for the enclosure (optional)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
