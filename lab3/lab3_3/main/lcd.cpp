#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

lcd::lcd() {}

void lcd::i2cWrite(uint8_t devAddr, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    //esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
/*
    if (ret != ESP_OK) {
        printf("I2C Write failed with error: %s\n", esp_err_to_name(ret));
    }
*/
}

    void lcd::init() {
        // No need to configure I2C here if it's already done in app_main
        sendCommand(0x38);
        vTaskDelay(pdMS_TO_TICKS(50));
    
        sendCommand(0x0C);
        vTaskDelay(pdMS_TO_TICKS(50));
    
        sendCommand(0x01);
        vTaskDelay(pdMS_TO_TICKS(50));
    
        sendCommand(0x06);
    
        setReg(0x00, 0x00);
        setReg(0x01, 0x00);
        setReg(0x08, 0xAA);
        setRGB(255, 255, 255);
    }
    

void lcd::sendCommand(uint8_t cmd) {
    uint8_t data[2] = {0x80, cmd};
    i2cWrite(LCD_ADDRESS, data, 2);
}

void lcd::sendData(uint8_t dataByte) {
    uint8_t data[2] = {0x40, dataByte};
    i2cWrite(LCD_ADDRESS, data, 2);
}

void lcd::setCursor(uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[] = {0x00, 0x40};
    sendCommand(0x80 | (col + row_offsets[row]));
}

void lcd::printstr(const char* str) {
    while (*str) {
        sendData(*str++);
    }
}

void lcd::setReg(uint8_t reg, uint8_t data) {
    uint8_t buffer[2] = {reg, data};
    i2cWrite(RGB_ADDRESS, buffer, 2);
}

void lcd::setRGB(uint8_t r, uint8_t g, uint8_t b) {
    setReg(0x00, 0x00);  // Mode1
    setReg(0x01, 0x00);  // Mode2
    setReg(0x08, 0xAA);  // Enable LEDs

    setReg(0x04, r);     // Red
    setReg(0x03, g);     // Green
    setReg(0x02, b);     // Blue
}
