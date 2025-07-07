# Lab 1.2 – ESP32 Hello World

Prints "Hello world!" and your name via UART using ESP-IDF on ESP32-C3.

## How It Works

- Uses ESP-IDF `hello_world` example.
- Target set to `esp32c3`.
- Added one `printf()` to include student name after the heap info.

## Build & Flash

```bash

. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32c3
idf.py build
idf.py flash monitor

---
```
## 🔸 Lab 1.3 – FreeRTOS LED Blink

```markdown
# Lab 1.3 – Blink LED (GPIO2)

A FreeRTOS-based app that blinks the onboard LED on GPIO2 once per second.

## How It Works

- Uses `xTaskCreate()` to spawn a task that toggles GPIO2.
- Delays 1000ms between toggles using `vTaskDelay`.

## Build & Flash

Same as Lab 1.2:

. $HOME/esp/esp-idf/export.sh
idf.py build
idf.py flash monitor
```
