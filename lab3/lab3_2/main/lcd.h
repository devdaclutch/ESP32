#pragma once

#include "driver/i2c.h"
#include "driver/gpio.h"
#include <stdint.h>

#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_SDA_IO   10
#define I2C_MASTER_SCL_IO   8
#define I2C_MASTER_FREQ_HZ  100000

#define LCD_ADDRESS         0x3E
#define RGB_ADDRESS         0x2D

class lcd {
public:
    lcd();
    void init();
    void setCursor(unsigned char col, unsigned char row);
    void printstr(const char* str);
    void setRGB(uint8_t r, uint8_t g, uint8_t b);

private:
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void setReg(uint8_t reg, uint8_t data);
    void i2cWrite(uint8_t devAddr, uint8_t* data, size_t len);
};
