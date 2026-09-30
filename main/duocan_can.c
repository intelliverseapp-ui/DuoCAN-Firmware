#include "duocan_can.h"
#include "esp_log.h"
#include "driver/twai.h"
#include "tcp_server.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "DUOCAN_CAN";

// ------------------------------------------------------------
// CAN INIT
// ------------------------------------------------------------
void duocan_can_init(void)
{
    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = GPIO_NUM_4,
        .rx_io = GPIO_NUM_5,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 16,
        .rx_queue_len = 16,
        .alerts_enabled = TWAI_ALERT_NONE,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        ESP_LOGI(TAG, "CAN driver installed");
    } else {
        ESP_LOGE(TAG, "CAN driver install FAILED");
    }

    if (twai_start() == ESP_OK) {
        ESP_LOGI(TAG, "CAN started");
    } else {
        ESP_LOGE(TAG, "CAN start FAILED");
    }
}

// ------------------------------------------------------------
// ENABLE CAN
// ------------------------------------------------------------
void duocan_enable_can(void)
{
    if (twai_start() == ESP_OK) {
        ESP_LOGI(TAG, "CAN enabled");
    } else {
        ESP_LOGE(TAG, "CAN enable FAILED");
    }
}

// ------------------------------------------------------------
// DISABLE CAN
// ------------------------------------------------------------
void duocan_disable_can(void)
{
    if (twai_stop() == ESP_OK) {
        ESP_LOGI(TAG, "CAN disabled");
    } else {
        ESP_LOGE(TAG, "CAN disable FAILED");
    }
}

// ------------------------------------------------------------
// SEND CAN FRAME
// ------------------------------------------------------------
void duocan_send_can_frame(uint32_t id, uint8_t dlc, uint8_t *data)
{
    twai_message_t msg;
    msg.identifier = id;
    msg.data_length_code = dlc;
    msg.flags = TWAI_MSG_FLAG_NONE;

    memcpy(msg.data, data, dlc);

    if (twai_transmit(&msg, pdMS_TO_TICKS(20)) == ESP_OK) {
        ESP_LOGI(TAG, "CAN TX ID=%" PRIu32 " DLC=%d", id, dlc);
    } else {
        ESP_LOGE(TAG, "CAN TX FAILED");
    }
}

// ------------------------------------------------------------
// STATUS
// ------------------------------------------------------------
void duocan_get_status(char *out, size_t out_len)
{
    twai_status_info_t status;
    twai_get_status_info(&status);

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
// CAN RX FORWARDING TASK
// ------------------------------------------------------------
void duocan_can_rx_forward_task(void *arg)
{
    twai_message_t msg;

    while (1) {
        if (twai_receive(&msg, pdMS_TO_TICKS(10)) == ESP_OK) {

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

            tcp_server_send_line(line);

            ESP_LOGI(TAG, "%s", line);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
