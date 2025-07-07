#include <string.h>
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

// —— I2C / SHTC3 sensor setup ——  
#define I2C_MASTER_SCL_IO         8  
#define I2C_MASTER_SDA_IO         10  
#define I2C_MASTER_NUM            I2C_NUM_0  
#define I2C_MASTER_FREQ_HZ        100000  
#define I2C_MASTER_TX_BUF_DISABLE 0  
#define I2C_MASTER_RX_BUF_DISABLE 0  
#define SHTC3_ADDR                0x70  
#define SHTC3_READ_CMD            0x7CA2  

static const char *TAG = "sensor_http";

static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode               = I2C_MODE_MASTER,
        .sda_io_num         = I2C_MASTER_SDA_IO,
        .scl_io_num         = I2C_MASTER_SCL_IO,
        .sda_pullup_en      = GPIO_PULLUP_ENABLE,
        .scl_pullup_en      = GPIO_PULLUP_ENABLE,
        .master.clk_speed   = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(
        I2C_MASTER_NUM,
        I2C_MODE_MASTER,
        I2C_MASTER_RX_BUF_DISABLE,
        I2C_MASTER_TX_BUF_DISABLE,
        0
    );
}

static esp_err_t shtc3_read_data(uint8_t* data, size_t len) {
    uint8_t cmd[2] = { (SHTC3_READ_CMD >> 8) & 0xFF, SHTC3_READ_CMD & 0xFF };
    esp_err_t err = i2c_master_write_to_device(
        I2C_MASTER_NUM, SHTC3_ADDR, cmd, sizeof(cmd),
        pdMS_TO_TICKS(1000)
    );
    if (err != ESP_OK) return err;
    return i2c_master_read_from_device(
        I2C_MASTER_NUM, SHTC3_ADDR, data, len,
        pdMS_TO_TICKS(1000)
    );
}

static float convert_temperature(uint16_t raw) {
    return (raw * 175.0f / 65535.0f) - 45.0f;
}

static float convert_humidity(uint16_t raw) {
    return (raw * 100.0f / 65535.0f);
}

// —— HTTP POST setup ——  
#define WEB_SERVER "172.20.10.1"   // ← set to your hotspot IP  
#define WEB_PORT   "8000"  
#define WEB_PATH   "/"  
static const char *REQUEST_FMT =
    "POST " WEB_PATH " HTTP/1.0\r\n"
    "Host: "WEB_SERVER ":" WEB_PORT "\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "\r\n"
    "%s";

void app_main(void)
{
    // 1) Init NVS, TCP/IP, Wi-Fi  
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    // 2) Init I2C  
    ESP_ERROR_CHECK(i2c_master_init());
    vTaskDelay(pdMS_TO_TICKS(100));  // sensor power-up delay

    // 2.5) Wake up SHTC3
    uint8_t wake_cmd[2] = { 0x35, 0x17 };  // wakeup command
    i2c_master_write_to_device(
        I2C_MASTER_NUM, SHTC3_ADDR, wake_cmd, sizeof(wake_cmd), pdMS_TO_TICKS(100)
    );
    vTaskDelay(pdMS_TO_TICKS(1));  // short pause after wakeup

    // 3) resolver info  
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    char recv_buf[64];

    while (1) {
        // 4) Read sensor
        uint8_t data[6] = {0};
        esp_err_t err = shtc3_read_data(data, sizeof(data));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        uint16_t raw_t = (data[0] << 8) | data[1];
        uint16_t raw_h = (data[3] << 8) | data[4];
        float temp_c = convert_temperature(raw_t);
        float hum = convert_humidity(raw_h);

        char post_data[64];
        int pd_len = snprintf(
            post_data, sizeof(post_data),
            "{\"temperature\":%.2f,\"humidity\":%.2f}",
            temp_c, hum
        );

        if (getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res) != 0) {
            ESP_LOGE(TAG, "DNS lookup failed");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int sock = socket(res->ai_family, res->ai_socktype, 0);
        if (sock < 0 || connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "Socket/connect failed");
            if (sock >= 0) close(sock);
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        freeaddrinfo(res);

        char request[256];
        int req_len = snprintf(
            request, sizeof(request),
            "POST " WEB_PATH " HTTP/1.0\r\n"
            "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n\r\n%s",
            pd_len, post_data
        );

        if (write(sock, request, req_len) < 0) {
            ESP_LOGE(TAG, "Send failed");
        }

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        while (read(sock, recv_buf, sizeof(recv_buf)-1) > 0) {}

        close(sock);
        ESP_LOGI(TAG, "Posted: T=%.2fC, H=%.2f%%", temp_c, hum);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
