# Lab 3 
## Lab 3.2 – RGB LCD1602 Display (Ported to ESP-IDF)

Reimplemented the DFRobot_RGBLCD1602 Arduino library in C++ for ESP-IDF.

### Features

- Maintains original API (e.g. `lcd.setCursor()`, `lcd.printstr()`).
- Internals use ESP-IDF I2C APIs.
- Displays:  Hello CSE121!
             [Your Last Name]

### Notes

- Written in C++.
- `Wire` and `Print` stripped out and replaced with native ESP32 equivalents.

## Lab 3.3 – Display Temperature and Humidity

Integrated SHTC3 sensor readings with RGB LCD1602 display.

### Display Layout

- Line 1: Celsius temperature
- Line 2: Humidity %

### Refresh Rate

- Data updates every 1 second.

### Notes

- Uses shared I2C bus for both sensor and display.
- Code builds on Lab 3.2 display library.
