# Lab 4.1 – Tilt Direction Detection

Reads tilt data from the ICM-42670-P IMU and logs direction (left/right/up/down) based on orientation.

## Notes

- No Bluetooth or output interaction yet — just serial logging of direction.

# Lab 4.2 – Bluetooth Mouse Emulation

Maps tilt direction from the ICM-42670-P sensor to mouse movement using ESP32 BLE HID.

## Features

- Directional tilt → cursor movement
- Bluetooth HID mouse emulation

# Lab 4.3 – Tilt-Based Acceleration Control

Builds on Lab 4.2. Mouse movement speed increases based on tilt strength and how long the board is tilted.

## Features

- Reads tilt magnitude
- Calculates movement delta
- Smooth, scalable cursor movement
