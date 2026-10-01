#include "duocan_can.h"
#include "esp_log.h"
#include "driver/twai.h"
#include "tcp_queue.h"
#include "duocan_leds.h"
#include "driver/gpio.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "DUOCAN_CAN";

// ------------------------------------------------------------
// TRANSCEIVER CONTROL PIN (ADJUST BASED ON BOARD)
// ------------------------------------------------------------
#define CAN_TRANSCEIVER_EN    GPIO_NUM_6

static void duocan_transceiver_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CAN_TRANSCEIVER_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    gpio_set_level(CAN_TRANSCEIVER_EN, 1);
    ESP_LOGI(TAG, "CAN transceiver enabled");
}

static void duocan_transceiver_disable(void)
{
    gpio_set_level(CAN_TRANSCEIVER_EN, 0);
    ESP_LOGI(TAG, "CAN transceiver disabled");
}

// ------------------------------------------------------------
// CAN INIT
// ------------------------------------------------------------
esp_err_t duocan_can_init(void)
{
    duocan_transceiver_init();

    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = GPIO_NUM_4,
        .rx_io = GPIO_NUM_5,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 16,
        .rx_queue_len = 16,
        .alerts_enabled = TWAI_ALERT_BUS_OFF |
                          TWAI_ALERT_RX_QUEUE_FULL |
                          TWAI_ALERT_ERR_PASS |
                          TWAI_ALERT_RX_DATA,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CAN driver install FAILED: %s", esp_err_to_name(err));
        return err;
    }

    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CAN start FAILED: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        return err;
    }

    ESP_LOGI(TAG, "CAN initialized and started");
    return ESP_OK;
}

// ------------------------------------------------------------
// ENABLE CAN
// ------------------------------------------------------------
esp_err_t duocan_enable_can(void)
{
    duocan_transceiver_init();
    esp_err_t err = twai_start();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAN enabled");
    } else {
        ESP_LOGE(TAG, "CAN enable FAILED: %s", esp_err_to_name(err));
    }
    return err;
}

// ------------------------------------------------------------
// DISABLE CAN
// ------------------------------------------------------------
esp_err_t duocan_disable_can(void)
{
    duocan_transceiver_disable();
    esp_err_t err = twai_stop();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAN disabled");
    } else {
        ESP_LOGE(TAG, "CAN disable FAILED: %s", esp_err_to_name(err));
    }
    return err;
}

// ------------------------------------------------------------
// SEND CAN FRAME (SAFE)
// ------------------------------------------------------------
esp_err_t duocan_send_can_frame(uint32_t id, uint8_t dlc, const uint8_t *data)
{
    if (dlc > 8) {
        ESP_LOGE(TAG, "Invalid DLC=%u (must be 0–8)", dlc);
        return ESP_ERR_INVALID_ARG;
    }

    if (id > 0x7FF) {
        ESP_LOGE(TAG, "Invalid CAN ID=%" PRIu32 " (standard ID max 0x7FF)", id);
        return ESP_ERR_INVALID_ARG;
    }

    twai_message_t msg = {
        .identifier = id,
        .data_length_code = dlc,
        .flags = TWAI_MSG_FLAG_NONE
    };

    memset(msg.data, 0, sizeof(msg.data));
    memcpy(msg.data, data, dlc);

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(20));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAN TX ID=%" PRIu32 " DLC=%u", id, dlc);
        duocan_leds_can_tx_active();   // LED2 = blue
    } else {
        ESP_LOGE(TAG, "CAN TX FAILED: %s", esp_err_to_name(err));
        duocan_leds_error();
    }

    return err;
}

// ------------------------------------------------------------
// STATUS
// ------------------------------------------------------------
void duocan_get_status(char *out, size_t out_len)
{
    twai_status_info_t status;
    esp_err_t err = twai_get_status_info(&status);

    if (err != ESP_OK) {
        snprintf(out, out_len, "CAN STATUS ERROR: %s\n", esp_err_to_name(err));
        return;
    }

    snprintf(out, out_len,
             "CAN STATUS:\n"
             "TX: %" PRIu32 "\n"
             "RX: %" PRIu32 "\n"
             "ERR: %" PRIu32 "\n"
             "STATE: %u\n",
             status.msgs_to_tx,
             status.msgs_to_rx,
             status.bus_error_count,
             status.state);
}

// ------------------------------------------------------------
// CAN RX FORWARDING TASK (ASYNC + QUEUED)
// ------------------------------------------------------------
void duocan_can_rx_forward_task(void *arg)
{
    twai_message_t msg;

    while (1) {
        esp_err_t err = twai_receive(&msg, pdMS_TO_TICKS(10));

        if (err == ESP_OK) {

            // LED2 = green (RX activity)
            duocan_leds_can_rx_active();

            char line[128];
            int n = snprintf(line, sizeof(line),
                             "CAN_RX %" PRIu32 " %u",
                             msg.identifier,
                             msg.data_length_code);

            for (int i = 0; i < msg.data_length_code; i++) {
                n += snprintf(line + n, sizeof(line) - n,
                              " %u", msg.data[i]);
            }

            n += snprintf(line + n, sizeof(line) - n, "\n");

            // QUEUED — not sent directly
            tcp_queue_push(line);

            ESP_LOGI(TAG, "%s", line);
        }
        else if (err == ESP_ERR_TIMEOUT) {
            // No frame — idle
            duocan_leds_can_idle();   // LED2 = dim white
        }
        else {
            ESP_LOGE(TAG, "CAN RX ERROR: %s", esp_err_to_name(err));
            duocan_leds_error();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
