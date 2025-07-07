#include <stdio.h>
#include <math.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"

// Constants
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

#define SHTC3_SENSOR_ADDR 0x70 

#define CMD_WAKEUP                  0x3517
#define CMD_SLEEP                   0xB098
#define CMD_READ_T_RH               0x7866

#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_SDA_IO   10
#define I2C_MASTER_SCL_IO   8
#define I2C_MASTER_FREQ_HZ  100000

#define LCD_ADDRESS         0x3E
#define RGB_ADDRESS         0x2D

// ---- Function Definitions ----
uint8_t calculate_crc(uint8_t data[], int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

esp_err_t shtc3_send_command(uint16_t cmd) {
    uint8_t write_buf[2] = { static_cast<uint8_t>(cmd >> 8), static_cast<uint8_t>(cmd & 0xFF) };
    return i2c_master_write_to_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR,
                                      write_buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t shtc3_read_data(uint16_t* temp_raw, uint16_t* hum_raw) {
    esp_err_t err = shtc3_send_command(CMD_READ_T_RH);
    if (err != ESP_OK) {
        printf("Failed to send read command: %s\n", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6];
    err = i2c_master_read_from_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR,
                                      data, 6, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        printf("Sensor read failed: %s\n", esp_err_to_name(err));
        return err;
    }

    if (calculate_crc(data, 2) != data[2] || calculate_crc(&data[3], 2) != data[5]) {
        printf("CRC check failed\n");
        return ESP_ERR_INVALID_CRC;
    }

    *temp_raw = (data[0] << 8) | data[1];
    *hum_raw  = (data[3] << 8) | data[4];
    return ESP_OK;
}

void i2c_master_init() {
    i2c_config_t conf = {};  // Zero-initialize the struct
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);
}

void i2c_scanner() {
    printf("Scanning I2C bus...\n");
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK) {
            printf("Device found at 0x%02X\n", addr);
        }
    }
}

// ---- MAIN LOOP ----
extern "C" void app_main() {
    i2c_master_init();
    i2c_scanner();

    lcd myLCD;
    myLCD.init();
    myLCD.setRGB(0, 255, 0);  // Set backlight to green

    while (1) {
        shtc3_send_command(CMD_WAKEUP);

        uint16_t raw_temp = 0, raw_hum = 0;
        if (shtc3_read_data(&raw_temp, &raw_hum) == ESP_OK) {
            float temp_C = -45 + 175 * (raw_temp / 65535.0);
            float humidity = 100 * (raw_hum / 65535.0);

            char line1[17];
            char line2[17];

            snprintf(line1, sizeof(line1), "Temp: %.0fC", round(temp_C));
            snprintf(line2, sizeof(line2), "Hum : %.0f%%", round(humidity));

            myLCD.setCursor(0, 0);
            myLCD.printstr(line1);

            myLCD.setCursor(0, 1);
            myLCD.printstr(line2);
        } else {
            printf("Sensor read error!\n");
        }

        shtc3_send_command(CMD_SLEEP);
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
    }
}
