# ESP32-C3 Embedded Systems Projects (CSE 121 - UCSC)

This repository contains labs and projects built using the ESP32-C3 development board for the CSE 121 course at UC Santa Cruz. Projects include I2C sensor integration, Bluetooth HID, Morse code transmission, and HTTP networking. All projects are written in C or C++ using the ESP-IDF framework.
## Prerequisites

You **must install the ESP-IDF framework** to build and flash any of the labs.

### Install ESP-IDF

```bash
# Clone the ESP-IDF repo (recursive is critical)
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf

# Run the installer
cd ~/esp/esp-idf
./install.sh esp32c3

# Export environment (do this every new terminal session)
. ./export.sh
```
## Labs Summary

| Lab      | Description                                                   |
|----------|---------------------------------------------------------------|
| Lab 1.2  | ESP32 Hello World (prints name)                               |
| Lab 1.3  | FreeRTOS LED blink using GPIO2                                |
| Lab 2.2  | SHTC3 temp/humidity sensor over I2C                           |
| Lab 3.1  | Solder headers for I2C connection                             |
| Lab 3.2  | Ported RGB LCD1602 display library to ESP-IDF (C++)          |
| Lab 3.3  | Display temp/humidity live on LCD                             |
| Lab 4.1  | Tilt detection via ICM-42670-P                                |
| Lab 4.2  | Bluetooth mouse control via IMU tilt                          |
| Lab 4.3  | Mouse acceleration control based on tilt strength             |
| Lab 5.1  | Morse code TX/RX using LED and photodiode                     |
| Lab 6.1  | Ultrasonic distance sensor with temperature correction        |
| Lab 7.1  | Fetch weather data from wttr.in                               |
| Lab 7.2  | Post sensor data to a local HTTP server                       |
| Lab 7.3  | Combine remote + local data and post back to server           |

## Environment

- Board: ESP32-C3
- Toolchain: ESP-IDF (latest)
- Language: C / C++, Python

Each lab folder includes:
- `main/` source directory
- `sdkconfig`, `CMakeLists.txt`, `README.md`
- Source code only — `build/` is excluded

---
## Build & Flash (for any lab)
```bash
. $HOME/esp/esp-idf/export.sh   # Set up env
idf.py set-target esp32c3       # Set chip target
idf.py build                    # Compile
idf.py flash                    # Flash to board
idf.py monitor                  # Serial output
Ctrl+] to exit monitor
```
