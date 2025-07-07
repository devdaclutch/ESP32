
## 🔸 Lab 2 – SHTC3 Sensor

```markdown
# Lab 2.2 – Temperature and Humidity via SHTC3

Reads temperature and humidity from an onboard SHTC3 sensor every 2 seconds.

## Features

- Uses I2C to talk to SHTC3.
- Displays Celsius, Fahrenheit, and % humidity over serial.
- Includes power on/off sequence and checksum validation.

## Notes

- Default I2C pins on ESP32-C3 were used.
- Checksum follows Sensirion's CRC-8 formula.
