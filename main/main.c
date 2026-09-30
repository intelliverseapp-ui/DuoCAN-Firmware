// main.c - DuoCAN ESP32-C6 firmware (Wi-Fi Access Point + CAN)
// DuoCAN Shield + Seeed XIAO ESP32-C6
// - CAN transceiver enable
// - DuoCAN Rev A RGB LEDs (split behavior)
// - TWAI CAN RX/TX
// - ESP32-C6 Wi-Fi Access Point (SSID: DuoCAN-C6)
// - TCP Server for BabyNodeAutomotive

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

#include "duocan_leds.h"
#include "tcp_server.h"
#include "duocan_can.h"

static const char *TAG = "DuoCAN";

// ---------------- WIFI ACCESS POINT ----------------

static void init_wifi_ap(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi AP...");

    // Initialize NVS (required for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP stack and default Wi-Fi AP interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    (void)netif;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "DuoCAN-C6",
            .ssid_len = 0,
            .channel = 1,
            .password = "duocan123",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started");
    ESP_LOGI(TAG, "SSID: DuoCAN-C6  PASSWORD: duocan123");
    ESP_LOGI(TAG, "Connect from Android and IP will be 192.168.4.1");
}

// ---------------- APP MAIN ----------------

void app_main(void)
{
    printf(">>> APP_MAIN ENTERED (DuoCAN) <<<\n");
    fflush(stdout);

    ESP_LOGI(TAG, "DuoCAN ESP32-C6 starting...");

    // Initialize DuoCAN Rev A LEDs
    duocan_leds_init();

    // Boot indicator = LED1 RED
    led1_set_red();
    led2_set_off();

    // CAN subsystem
    duocan_can_init();

    // Start CAN RX forwarding task
    xTaskCreate(duocan_can_rx_forward_task,
                "can_rx_forward",
                4096,
                NULL,
                5,
                NULL);

    // Wi-Fi Access Point
    init_wifi_ap();

    // System ready = LED1 GREEN
    led1_set_green();
    led2_set_off();

    // TCP Server task
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "DuoCAN ready (Wi-Fi AP + CAN + TCP Server + CAN RX Forwarding)");

    // Idle loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
