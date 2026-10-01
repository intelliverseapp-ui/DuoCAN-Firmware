#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "esp_random.h"

#include "duocan_leds.h"
#include "tcp_server.h"
#include "tcp_queue.h"
#include "duocan_can.h"

static const char *TAG = "DuoCAN";

// ------------------------------------------------------------
// DISABLE CAN BURST GENERATOR (IMPORTANT)
// ------------------------------------------------------------
#define ENABLE_CAN_BURST_TEST   0   // <—— DISABLED

// ------------------------------------------------------------
// CAN BURST GENERATOR TASK (RX-SIMULATION)
// ------------------------------------------------------------
static void can_burst_generator_task(void *arg)
{
    ESP_LOGW(TAG, "CAN BURST GENERATOR ACTIVE — SIMULATING VEHICLE TRAFFIC (RX-ONLY)");

    uint32_t fake_id = 0x100;

    while (1) {

        if (tcp_outbound_queue &&
            uxQueueMessagesWaiting(tcp_outbound_queue) > (TCP_QUEUE_LENGTH - 4)) {

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint8_t data[8];
        for (int i = 0; i < 8; i++) {
            data[i] = (uint8_t)(esp_random() & 0xFF);
        }

        char line[256];
        snprintf(
            line,
            sizeof(line),
            "CAN_RX %lu 8 %02X %02X %02X %02X %02X %02X %02X %02X",
            (unsigned long)fake_id,
            data[0], data[1], data[2], data[3],
            data[4], data[5], data[6], data[7]
        );

        tcp_queue_push(line);

        fake_id++;
        if (fake_id > 0x180) {
            fake_id = 0x100;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ------------------------------------------------------------
// WIFI ACCESS POINT
// ------------------------------------------------------------
static esp_err_t init_wifi_ap(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi AP...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    (void)netif;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "DuoCAN-C6",
            .ssid_len = 0,
            .channel = 1,
            .password = "duocan123",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = { .required = false },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started");
    ESP_LOGI(TAG, "SSID: DuoCAN-C6  PASSWORD: duocan123");
    ESP_LOGI(TAG, "Connect from Android and IP will be 192.168.4.1");

    return ESP_OK;
}

// ------------------------------------------------------------
// APP MAIN
// ------------------------------------------------------------
void app_main(void)
{
    // Enable verbose logging for TCP server
    esp_log_level_set("TCP", ESP_LOG_VERBOSE);

    printf(">>> APP_MAIN ENTERED (DuoCAN) <<<\n");
    fflush(stdout);

    ESP_LOGI(TAG, "DuoCAN ESP32-C6 starting...");

    duocan_leds_init();

    led1_set_red();
    led2_set_off();

    tcp_queue_init();

    esp_err_t can_ret = duocan_can_init();
    if (can_ret != ESP_OK) {
        ESP_LOGE(TAG, "CAN init FAILED: %s", esp_err_to_name(can_ret));
        led1_set_red();
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    BaseType_t can_task_ret = xTaskCreate(
        duocan_can_rx_forward_task,
        "can_rx_forward",
        4096,
        NULL,
        5,
        NULL
    );

    if (can_task_ret != pdPASS) {
        ESP_LOGE(TAG, "CAN RX task creation FAILED");
        led1_set_red();
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

#if ENABLE_CAN_BURST_TEST
    xTaskCreate(
        can_burst_generator_task,
        "can_burst_gen",
        4096,
        NULL,
        5,
        NULL
    );
#endif

    esp_err_t wifi_ret = init_wifi_ap();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi AP init FAILED: %s", esp_err_to_name(wifi_ret));
        led1_set_red();
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    led1_set_green();
    led2_set_off();

    // ⭐ Raise TCP server priority to avoid starvation
    BaseType_t tcp_task_ret = xTaskCreate(
        tcp_server_task,
        "tcp_server",
        4096,
        NULL,
        10,     // <—— PRIORITY BOOSTED
        NULL
    );

    if (tcp_task_ret != pdPASS) {
        ESP_LOGE(TAG, "TCP server task creation FAILED");
        led1_set_red();
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "DuoCAN ready (Wi-Fi AP + CAN + TCP Queue + TCP Server)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
