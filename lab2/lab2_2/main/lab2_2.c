#include <stdio.h>
#include <math.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_SCL_IO           8
#define I2C_MASTER_SDA_IO           10
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

#define SHTC3_SENSOR_ADDR 0x70 



// SHTC3 Commands
#define CMD_WAKEUP                  0x3517
#define CMD_SLEEP                   0xB098
#define CMD_READ_T_RH               0x7866

// CRC Polynomial for SHTC3 is 0x31 (x⁸ + x⁵ + x⁴ + 1)
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
    uint8_t write_buf[2] = { cmd >> 8, cmd & 0xFF };
    return i2c_master_write_to_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR,
                                      write_buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t shtc3_read_data(uint16_t* temp_raw, uint16_t* hum_raw) {
    // Send measurement trigger
    esp_err_t err = shtc3_send_command(CMD_READ_T_RH);
    if (err != ESP_OK) {
        printf("Failed to send read command: %s\n", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(20));  // Datasheet recommends ~15ms

    uint8_t data[6];
    err = i2c_master_read_from_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR,
                                      data, 6, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        printf("Sensor read failed: %s\n", esp_err_to_name(err));
        return err;
    }

    // Check CRCs
    if (calculate_crc(data, 2) != data[2] || calculate_crc(&data[3], 2) != data[5]) {
        printf("CRC check failed\n");
        return ESP_ERR_INVALID_CRC;
    }

    *temp_raw = (data[0] << 8) | data[1];
    *hum_raw  = (data[3] << 8) | data[4];
    return ESP_OK;
}


void print_temperature_and_humidity() {
    shtc3_send_command(CMD_WAKEUP);
    vTaskDelay(pdMS_TO_TICKS(1));  // wake-up delay

    uint16_t raw_temp, raw_hum;
    esp_err_t raw = shtc3_read_data(&raw_temp, &raw_hum);

    //shtc3_send_command(CMD_SLEEP);  // always power down

    /*
    if (err != ESP_OK) {
        printf("Sensor read failed: %s\n", esp_err_to_name(err));
        return;
    }*/
    

    // Convert raw values to physical values (from datasheet)
    float temp_C = -45 + 175 * (raw_temp / 65535.0);
    float temp_F = temp_C * 9.0 / 5.0 + 32.0;
    float humidity = 100 * (raw_hum / 65535.0);
    //printf("Raw temp: 0x%04X, Raw hum: 0x%04X\n", raw_temp, raw_hum);

    printf("Temperature is %.0fC (or %.0fF) with a %.0f%% humidity\n",
           round(temp_C), round(temp_F), round(humidity));
}

void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
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


void app_main() {
    i2c_master_init();
    i2c_scanner(); 
    while (1) {
        shtc3_send_command(CMD_WAKEUP);
        print_temperature_and_humidity();
        shtc3_send_command(CMD_SLEEP); 
        vTaskDelay(pdMS_TO_TICKS(1000));  // Print every 5 seconds
    }
}

