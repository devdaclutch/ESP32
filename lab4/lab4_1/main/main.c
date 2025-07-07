/* FILE: main/main.c */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "icm42670.h"

#define TAG "TILT"

#define SDA_PIN 10
#define SCL_PIN 8
#define ACCEL_THRESHOLD 1500

static icm42670_handle_t icm = NULL;

void app_main() {
    // 1. Create I2C bus
    i2c_master_bus_config_t i2c_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = SCL_PIN,
        .sda_io_num = SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &bus));

    // 2. Create sensor handle
    ESP_ERROR_CHECK(icm42670_create(bus, ICM42670_I2C_ADDRESS, &icm));

    // 3. Power ON accelerometer and set config
    ESP_ERROR_CHECK(icm42670_acce_set_pwr(icm, ACCE_PWR_LOWNOISE));
    ESP_ERROR_CHECK(icm42670_gyro_set_pwr(icm, GYRO_PWR_OFF));

    icm42670_cfg_t cfg = {
        .acce_fs = ACCE_FS_4G,
        .acce_odr = ACCE_ODR_100HZ,
        .gyro_fs = GYRO_FS_250DPS,
        .gyro_odr = GYRO_ODR_100HZ,
    };
    ESP_ERROR_CHECK(icm42670_config(icm, &cfg));

    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 4. Main tilt loop
    while (1) {
        icm42670_raw_value_t raw;
        if (icm42670_get_acce_raw_value(icm, &raw) != ESP_OK) {
            ESP_LOGE(TAG, "Accel read failed");
            continue;
        }

        int16_t ax = raw.x;
        int16_t ay = raw.y;
        

        char direction[32] = "";
        if (ay > ACCEL_THRESHOLD) strcat(direction, "UP ");
        if (ay < -ACCEL_THRESHOLD) strcat(direction, "DOWN ");
        if (ax > ACCEL_THRESHOLD) strcat(direction, "RIGHT ");
        if (ax < -ACCEL_THRESHOLD) strcat(direction, "LEFT ");

        if (direction[0]) {
            direction[strlen(direction) - 1] = '\0';
            ESP_LOGI(TAG, "%s", direction);
        } else {
            ESP_LOGI(TAG, "FLAT");
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
