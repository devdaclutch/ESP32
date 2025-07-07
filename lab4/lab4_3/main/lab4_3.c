/* FILE: main/main.c */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_hidd_prf_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "driver/i2c_master.h"

#define MOUSE_LEFT_BUTTON 0x01

#include "icm42670.h"
#include "hid_dev.h"

#define TAG "TILT_MOUSE"

// I2C
#define SDA_PIN 10
#define SCL_PIN 8
#define ACCEL_THRESH_BIT 1500
#define ACCEL_THRESH_LOT 4000

#define STEP_BIT 2
#define STEP_LOT 6
#define REPORT_DELAY_MS 20

static icm42670_handle_t icm = NULL;
static uint16_t hid_conn_id = 0;
static bool sec_conn = false;

// HID
static uint8_t hidd_service_uuid128[] = {
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x03c0,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x30,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
    switch (event) {
        case ESP_HIDD_EVENT_REG_FINISH:
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                esp_ble_gap_set_device_name("MouseDev");
                esp_ble_gap_config_adv_data(&hidd_adv_data);
            }
            break;
        case ESP_HIDD_EVENT_BLE_CONNECT:
            ESP_LOGI(TAG, "BLE connected");
            hid_conn_id = param->connect.conn_id;
            sec_conn = true;
            break;
        case ESP_HIDD_EVENT_BLE_DISCONNECT:
            ESP_LOGI(TAG, "BLE disconnected");
            sec_conn = false;
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        default:
            break;
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;

        case ESP_GAP_BLE_SEC_REQ_EVT:
            ESP_LOGI(TAG, "Security request received, setting encryption...");
            esp_ble_set_encryption(param->ble_security.ble_req.bd_addr, ESP_BLE_SEC_ENCRYPT_MITM);
            break;

        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            sec_conn = param->ble_security.auth_cmpl.success;
            if (sec_conn) {
                ESP_LOGI(TAG, "Authentication successful");
            } else {
                ESP_LOGE(TAG, "Auth failed, reason: 0x%x", param->ble_security.auth_cmpl.fail_reason);
            }
            break;

        default:
            break;
    }
}


// Tilt Mouse Logic with Auto-Click
void tilt_mouse_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for BLE stack

    static int accel_multiplier_x = 1;
    static int accel_multiplier_y = 1;
    static int still_counter = 0;
    static bool clicked = false;

    while (1) {
        if (!sec_conn) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        icm42670_raw_value_t raw;
        if (icm42670_get_acce_raw_value(icm, &raw) != ESP_OK) {
            ESP_LOGE(TAG, "Accel read failed");
            continue;
        }

        int16_t ax = raw.x;
        int16_t ay = raw.y;
        int x_delta = 0;
        int y_delta = 0;

        // LEFT/RIGHT
        if (ax > ACCEL_THRESH_LOT) {
            x_delta = STEP_LOT / 2;
            accel_multiplier_x++;
        } else if (ax > ACCEL_THRESH_BIT) {
            x_delta = STEP_BIT / 2;
            accel_multiplier_x++;
        } else if (ax < -ACCEL_THRESH_LOT) {
            x_delta = -STEP_LOT / 2;
            accel_multiplier_x++;
        } else if (ax < -ACCEL_THRESH_BIT) {
            x_delta = -STEP_BIT / 2;
            accel_multiplier_x++;
        } else {
            x_delta = 0;
            accel_multiplier_x = 1;
        }

        // UP/DOWN
        if (ay > ACCEL_THRESH_LOT) {
            y_delta = -STEP_LOT / 2;
            accel_multiplier_y++;
        } else if (ay > ACCEL_THRESH_BIT) {
            y_delta = -STEP_BIT / 2;
            accel_multiplier_y++;
        } else if (ay < -ACCEL_THRESH_LOT) {
            y_delta = STEP_LOT / 2;
            accel_multiplier_y++;
        } else if (ay < -ACCEL_THRESH_BIT) {
            y_delta = STEP_BIT / 2;
            accel_multiplier_y++;
        } else {
            y_delta = 0;
            accel_multiplier_y = 1;
        }

        // Clamp acceleration
        if (accel_multiplier_x > 3) accel_multiplier_x = 3;
        if (accel_multiplier_y > 3) accel_multiplier_y = 3;

        x_delta *= accel_multiplier_x;
        y_delta *= accel_multiplier_y;

        if (x_delta != 0 || y_delta != 0) {
            esp_hidd_send_mouse_value(hid_conn_id, 0, x_delta, y_delta);
            ESP_LOGI(TAG, "Move X:%d Y:%d", x_delta, y_delta);
            still_counter = 0;
            clicked = false;
        } else {
            esp_hidd_send_mouse_value(hid_conn_id, 0, 0, 0);
            still_counter++;
            if (still_counter >= 50 && !clicked) { // ~1s
                ESP_LOGI(TAG, "Auto-click triggered");
                esp_hidd_send_mouse_value(hid_conn_id, MOUSE_LEFT_BUTTON, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(30));
                esp_hidd_send_mouse_value(hid_conn_id, 0, 0, 0);
                clicked = true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(REPORT_DELAY_MS));
    }
}


void app_main(void) {
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Init I2C + Sensor
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
    ESP_ERROR_CHECK(icm42670_create(bus, ICM42670_I2C_ADDRESS, &icm));
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

    // Init Bluetooth
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_hidd_profile_init());
    esp_ble_gap_register_callback(gap_event_handler);
    esp_hidd_register_callbacks(hidd_event_callback);

    // Security params
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    // Start tilt task
    xTaskCreate(&tilt_mouse_task, "tilt_mouse", 4096, NULL, 5, NULL);
}
