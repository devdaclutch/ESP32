#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#define TAG                   "MORSE_READER"
#define ADC_CHANNEL           ADC_CHANNEL_0
#define THRESHOLD             72

#define DOT_DURATION_MS       300
#define INTER_CHAR_GAP_MS     600
#define INTER_WORD_GAP_MS    1200
#define TRANSMISSION_END_MS  3000

#define MORSE_BUF_SIZE        16
#define MESSAGE_BUF_SIZE     128

typedef struct {
    const char *morse;
    char letter;
} MorseMap;

static const MorseMap morse_table[] = {
    {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},  {".", 'E'},
    {"..-.", 'F'}, {"--.", 'G'},  {"....", 'H'}, {"..", 'I'},   {".---", 'J'},
    {"-.-", 'K'},  {".-..", 'L'}, {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},
    {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'},  {"...", 'S'},  {"-", 'T'},
    {"..-", 'U'},  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'}, {"-.--", 'Y'},
    {"--..", 'Z'}, {"-----", '0'}, {".----", '1'}, {"..---", '2'},
    {"...--", '3'}, {"....-", '4'}, {".....", '5'}, {"-....", '6'},
    {"--...", '7'}, {"---..", '8'}, {"----.", '9'}
};

static char morse_to_char(const char *code) {
    for (size_t i = 0; i < sizeof(morse_table)/sizeof(morse_table[0]); ++i) {
        if (strcmp(code, morse_table[i].morse) == 0) {
            return morse_table[i].letter;
        }
    }
    return '?';
}

static void append_symbol(char *buf, size_t buf_size, char sym) {
    size_t len = strlen(buf);
    if (len + 1 < buf_size) {
        buf[len]     = sym;
        buf[len + 1] = '\0';
    }
}

static void flush_morse_buf(char *morse_buf, char *msg_buf) {
    if (morse_buf[0] == '\0') return;
    char decoded = morse_to_char(morse_buf);
    if (decoded != '?') {
        size_t msg_len = strlen(msg_buf);
        if (msg_len + 1 < MESSAGE_BUF_SIZE) {
            msg_buf[msg_len]     = decoded;
            msg_buf[msg_len + 1] = '\0';
        }
    }
    morse_buf[0] = '\0';
}

void app_main(void)
{

    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg));

    int last_state = 0;
    TickType_t last_t = xTaskGetTickCount();
    bool char_gap_handled = false, word_gap_handled = false;

    char morse_buf[MORSE_BUF_SIZE]   = {0};
    char message[MESSAGE_BUF_SIZE]   = {0};

    while (1) {
        int adc_val;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_val));
        int state = (adc_val > THRESHOLD);

        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = (now - last_t) * portTICK_PERIOD_MS;

        // on state change:
        if (state != last_state) {
            // rising edge → check gaps
            if (state == 1) {
                if (!word_gap_handled && elapsed_ms >= INTER_WORD_GAP_MS) {
                    flush_morse_buf(morse_buf, message);
                    strcat(message, " ");
                    word_gap_handled = true;
                }
                else if (!char_gap_handled && elapsed_ms >= INTER_CHAR_GAP_MS) {
                    flush_morse_buf(morse_buf, message);
                    char_gap_handled = true;
                }
                char_gap_handled = word_gap_handled = false;
            }
            else {
                if (elapsed_ms < DOT_DURATION_MS) {
                    append_symbol(morse_buf, MORSE_BUF_SIZE, '.');
                } else {
                    append_symbol(morse_buf, MORSE_BUF_SIZE, '-');
                }
            }
            last_state = state;
            last_t     = now;
        }
        else {
            if (state == 0 && elapsed_ms >= TRANSMISSION_END_MS) {
                flush_morse_buf(morse_buf, message);
                last_t = now;
            }
        }

        ESP_LOGI(TAG, "ADC: %4d | Morse: %-8s | Msg: %s",
                 adc_val, morse_buf, message);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}