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
