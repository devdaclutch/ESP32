extern "C" {
    void app_main();
}

#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

void app_main() {
    lcd myLCD;

    myLCD.init();

    myLCD.setCursor(0, 0);
    myLCD.printstr("Hello CSE121!");

    myLCD.setCursor(0, 1);
    myLCD.printstr("Chodavadiya");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
