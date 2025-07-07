#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO     8
#define I2C_MASTER_SDA_IO     10
#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_FREQ_HZ    100000
#define SHTC3_SENSOR_ADDR     0x70
#define CMD_WAKEUP            0x3517
#define CMD_SLEEP             0xB098
#define CMD_MEASURE_T_RH_CS   0x7CA2

#define SERVER_IP "172.20.10.1"  // Your Pi/iSH server IP
#define SERVER_PORT "1234"

static const char *TAG = "ESP32_COMBO";

// CRC helper for SHTC3
static uint8_t crc8(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

// I2C commands for SHTC3
static esp_err_t write_cmd(uint16_t cmd) {
    uint8_t buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_write_to_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR, buf, 2, pdMS_TO_TICKS(50));
}

static esp_err_t read_n(uint8_t *buf, size_t n) {
    return i2c_master_read_from_device(I2C_MASTER_NUM, SHTC3_SENSOR_ADDR, buf, n, pdMS_TO_TICKS(50));
}

static esp_err_t measure_shtc3(float *t_c, float *rh) {
    esp_err_t r;
    if ((r = write_cmd(CMD_WAKEUP)) != ESP_OK) return r;
    vTaskDelay(pdMS_TO_TICKS(2));

    if ((r = write_cmd(CMD_MEASURE_T_RH_CS)) != ESP_OK) {
        write_cmd(CMD_SLEEP);
        return r;
    }
    vTaskDelay(pdMS_TO_TICKS(12));

    uint8_t buf[6];
    if ((r = read_n(buf, 6)) != ESP_OK) {
        write_cmd(CMD_SLEEP);
        return r;
    }

    if (crc8(buf,2) != buf[2] || crc8(buf+3,2) != buf[5]) {
        write_cmd(CMD_SLEEP);
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t rawT = (buf[0] << 8) | buf[1];
    uint16_t rawRH = (buf[3] << 8) | buf[4];
    *t_c = -45.0f + 175.0f * rawT / 65535.0f;
    *rh = 100.0f * rawRH / 65535.0f;

    write_cmd(CMD_SLEEP);
    return ESP_OK;
}

// Replace spaces and newlines with '+'
static void url_encode_spaces(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ' ' || str[i] == '\n') {
            str[i] = '+';
        }
    }
}

// Simple GET request helper with special curl-like headers for wttr.in
static int simple_http_get(const char *host, const char *port, const char *path, char *response, size_t max_len, bool is_wttr) {
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res;
    int s, r, total = 0;

    if (getaddrinfo(host, port, &hints, &res) != 0 || res == NULL) return -1;
    s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) { freeaddrinfo(res); return -1; }
    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) { close(s); freeaddrinfo(res); return -1; }

    char req[512];
    if (is_wttr) {
        snprintf(req, sizeof(req),
            "GET %s HTTP/1.0\r\n"
            "Host: %s:80\r\n"
            "User-Agent: esp-idf/1.0 esp32 curl\r\n"
            "\r\n",
            path, host);
    } else {
        snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: ESP32Client/1.0\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host);
    }

    // ESP_LOGI(TAG, "HTTP Request:\n%s", req);
    write(s, req, strlen(req));

    char buf[128];
    while ((r = read(s, buf, sizeof(buf)-1)) > 0 && total < max_len-1) {
        memcpy(response + total, buf, r);
        total += r;
    }
    response[total] = 0;

    // ESP_LOGI(TAG, "Raw HTTP response:\n%s", response);

    close(s);
    freeaddrinfo(res);
    return 0;
}

// Main task: read sensors, get location & weather, POST data
static void combo_task(void *pvParams) {
    while (1) {
        float t_local, h_local;
        if (measure_shtc3(&t_local, &h_local) != ESP_OK) {
            // ESP_LOGE(TAG, "Sensor read failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        char location[128] = {0}, outdoor[512] = {0};
        if (simple_http_get(SERVER_IP, SERVER_PORT, "/location", location, sizeof(location), false) != 0) {
            ESP_LOGE(TAG, "Failed to get location");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        char *loc_body = strstr(location, "\r\n\r\n");
        if (!loc_body) { ESP_LOGE(TAG, "Invalid location response"); continue; }
        loc_body += 4;
        url_encode_spaces(loc_body);

        char wttr_path[128];
        snprintf(wttr_path, sizeof(wttr_path), "/%s?format=%%t", loc_body);
        if (simple_http_get("wttr.in", "80", wttr_path, outdoor, sizeof(outdoor), true) != 0) {
            ESP_LOGE(TAG, "Failed to get outdoor temp");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        char *wttr_body = outdoor;
        if ((strstr(outdoor, "200 OK") != NULL)) {
            wttr_body = strrchr(outdoor, '\n');
            if (wttr_body) wttr_body++;
            else { ESP_LOGE(TAG, "Couldn't find weather data"); continue; }
        } else {
            ESP_LOGE(TAG, "Invalid wttr.in response");
            continue;
        }

        // Build JSON payload
        char post_data[256];
        snprintf(post_data, sizeof(post_data),
                 "{\"location\":\"%s\",\"outdoor_temp\":\"%s\",\"local_temp\":%.2f,\"humidity\":%.2f}",
                 loc_body, wttr_body, t_local, h_local);
        ESP_LOGI(TAG, "Sending: %s", post_data);

        // POST to server
        struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
        struct addrinfo *res;
        int s = -1;
        if (getaddrinfo(SERVER_IP, SERVER_PORT, &hints, &res) == 0 && res) {
            s = socket(res->ai_family, res->ai_socktype, 0);
            if (s >= 0 && connect(s, res->ai_addr, res->ai_addrlen) == 0) {
                char req[512];
                snprintf(req, sizeof(req),
                         "POST / HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                         SERVER_IP, strlen(post_data), post_data);
                write(s, req, strlen(req));
                close(s);
            }
            freeaddrinfo(res);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));  // 5-second interval
    }
}

static void i2c_init() {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, cfg.mode, 0, 0, 0));
}

void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    i2c_init();
    xTaskCreate(&combo_task, "combo_task", 8192, NULL, 5, NULL);
}