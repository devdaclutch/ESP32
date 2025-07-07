#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"  // for esp_rom_delay_us

#define I2C_MASTER_SCL_IO           8
#define I2C_MASTER_SDA_IO           10
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

#define SHTC3_SENSOR_ADDR 0x70
#define CMD_WAKEUP                  0x3517
#define CMD_SLEEP                   0xB098
#define CMD_READ_T_RH               0x7866

#define TRIGGER_GPIO GPIO_NUM_2
#define ECHO_GPIO GPIO_NUM_3


uint8_t calculate_crc(uint8_t data[], int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
    return crc;
}

esp_err_t shtc3_send_command(uint16_t cmd) {
    uint8_t write_buf[2] = { cmd >> 8, cmd & 0xFF };
    return i2c_master_write_to_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR,
                                      write_buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t shtc3_read_data(uint16_t* temp_raw, uint16_t* hum_raw) {
    esp_err_t err = shtc3_send_command(CMD_READ_T_RH);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(20));  // wait for measurement

    uint8_t data[6];
    err = i2c_master_read_from_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR,
                                      data, 6, pdMS_TO_TICKS(100));
    if (err != ESP_OK) return err;

    if (calculate_crc(data, 2) != data[2] || calculate_crc(&data[3], 2) != data[5])
        return ESP_ERR_INVALID_CRC;

    *temp_raw = (data[0] << 8) | data[1];
    *hum_raw  = (data[3] << 8) | data[4];
    return ESP_OK;
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

float get_temperature_celsius() {
    uint16_t raw_temp, raw_hum;

    shtc3_send_command(CMD_WAKEUP);
    vTaskDelay(pdMS_TO_TICKS(1));
    esp_err_t err = shtc3_read_data(&raw_temp, &raw_hum);

    if (err != ESP_OK) {
       // printf("First temp read failed: %s. Retrying...\n", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(10));
        err = shtc3_read_data(&raw_temp, &raw_hum);
    }

    if (err != ESP_OK) {
       // printf("Second read failed: %s. Resetting I2C driver...\n", esp_err_to_name(err));
        shtc3_send_command(CMD_SLEEP);
        i2c_driver_delete(I2C_MASTER_NUM);
        i2c_master_init();
        shtc3_send_command(CMD_WAKEUP);
        vTaskDelay(pdMS_TO_TICKS(1));
        err = shtc3_read_data(&raw_temp, &raw_hum);
        if (err != ESP_OK) {
            shtc3_send_command(CMD_SLEEP);
            //printf("Final read failed: %s. Giving up.\n", esp_err_to_name(err));
            return -1;
        }
    }

    shtc3_send_command(CMD_SLEEP);
    return -45 + 175 * (raw_temp / 65535.0);
}



float get_speed_of_sound(float temp_celsius) {
    return 0.03313 + 0.0000606 * temp_celsius; // cm/us
}

uint32_t measure_echo_time_us() {
    int64_t timeout = esp_timer_get_time() + 30000;  // 30 ms max wait

    // Wait for echo to go HIGH
    while (gpio_get_level(ECHO_GPIO) == 0) {
        if (esp_timer_get_time() > timeout) {
           // printf("Timeout waiting for echo HIGH\n");
            return 0;
        }
    }

    int64_t start = esp_timer_get_time();

    // Wait for echo to go LOW
    timeout = start + 30000;
    
    while (gpio_get_level(ECHO_GPIO) == 1) {
        if (esp_timer_get_time() > timeout) {
           // printf("Timeout waiting for echo LOW\n");
            return 0;
        }
    }

    int64_t end = esp_timer_get_time();
    return (uint32_t)(end - start);
}

float measure_distance_cm(float temp_celsius) {
    gpio_set_level(TRIGGER_GPIO, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIGGER_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIGGER_GPIO, 0);

    uint32_t echo_time = measure_echo_time_us();
    if (echo_time == 0) return 0.0;

    float v = get_speed_of_sound(temp_celsius);
    return (echo_time * v) / 2.0;
}

void app_main() {
    i2c_master_init();
    gpio_set_direction(TRIGGER_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);

    while (1) {
        float temp_c = get_temperature_celsius();
        if (temp_c < 0 || temp_c > 50) {
           // printf("Temperature read failed or out of range: %.2f\n", temp_c);
        } else {
            float dist = measure_distance_cm(temp_c);
            if (dist > 0.0) {
                printf("Distance: %.2f cm at %.1fC\n", dist, temp_c);
            } else {
               //printf("No echo received (distance = 0)\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

