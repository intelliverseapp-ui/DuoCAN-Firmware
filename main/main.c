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
#include "duocan_commands.h"

#include "esp_http_server.h"   // HTTP server for iOS/Siri/Shortcuts

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
// SIMPLE HTTP HANDLERS (FOR iOS / SIRI / SHORTCUTS)
// ------------------------------------------------------------

// /ping  -> "PONG\n"
static esp_err_t http_ping_handler(httpd_req_t *req)
{
    const char *resp = "PONG\n";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// /status -> same as TCP STATUS
static esp_err_t http_status_handler(httpd_req_t *req)
{
    char buf[256];
    duocan_get_status(buf, sizeof(buf));
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

// /enable_can
static esp_err_t http_enable_can_handler(httpd_req_t *req)
{
    duocan_enable_can();
    const char *resp = "CAN ENABLED\n";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// /disable_can
static esp_err_t http_disable_can_handler(httpd_req_t *req)
{
    duocan_disable_can();
    const char *resp = "CAN DISABLED\n";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// ------------------------------------------------------------
// HIGH-LEVEL COMMAND HTTP HANDLERS (CALL INTO duocan_commands.c)
// ------------------------------------------------------------

// Doors
static esp_err_t http_lock_handler(httpd_req_t *req)
{
    duocan_cmd_lock_doors();
    httpd_resp_sendstr(req, "LOCK_DOORS\n");
    return ESP_OK;
}

static esp_err_t http_unlock_handler(httpd_req_t *req)
{
    duocan_cmd_unlock_doors();
    httpd_resp_sendstr(req, "UNLOCK_DOORS\n");
    return ESP_OK;
}

// Windows
static esp_err_t http_windows_down_handler(httpd_req_t *req)
{
    duocan_cmd_windows_down();
    httpd_resp_sendstr(req, "WINDOWS_DOWN\n");
    return ESP_OK;
}

static esp_err_t http_windows_up_handler(httpd_req_t *req)
{
    duocan_cmd_windows_up();
    httpd_resp_sendstr(req, "WINDOWS_UP\n");
    return ESP_OK;
}

// Sunroof
static esp_err_t http_sunroof_open_handler(httpd_req_t *req)
{
    duocan_cmd_sunroof_open();
    httpd_resp_sendstr(req, "SUNROOF_OPEN\n");
    return ESP_OK;
}

static esp_err_t http_sunroof_close_handler(httpd_req_t *req)
{
    duocan_cmd_sunroof_close();
    httpd_resp_sendstr(req, "SUNROOF_CLOSE\n");
    return ESP_OK;
}

static esp_err_t http_sunroof_vent_handler(httpd_req_t *req)
{
    duocan_cmd_sunroof_vent();
    httpd_resp_sendstr(req, "SUNROOF_VENT\n");
    return ESP_OK;
}

// Lights
static esp_err_t http_headlights_on_handler(httpd_req_t *req)
{
    duocan_cmd_headlights_on();
    httpd_resp_sendstr(req, "HEADLIGHTS_ON\n");
    return ESP_OK;
}

static esp_err_t http_headlights_off_handler(httpd_req_t *req)
{
    duocan_cmd_headlights_off();
    httpd_resp_sendstr(req, "HEADLIGHTS_OFF\n");
    return ESP_OK;
}

static esp_err_t http_fog_on_handler(httpd_req_t *req)
{
    duocan_cmd_fog_lights_on();
    httpd_resp_sendstr(req, "FOG_LIGHTS_ON\n");
    return ESP_OK;
}

static esp_err_t http_fog_off_handler(httpd_req_t *req)
{
    duocan_cmd_fog_lights_off();
    httpd_resp_sendstr(req, "FOG_LIGHTS_OFF\n");
    return ESP_OK;
}

static esp_err_t http_interior_on_handler(httpd_req_t *req)
{
    duocan_cmd_interior_lights_on();
    httpd_resp_sendstr(req, "INTERIOR_LIGHTS_ON\n");
    return ESP_OK;
}

static esp_err_t http_interior_off_handler(httpd_req_t *req)
{
    duocan_cmd_interior_lights_off();
    httpd_resp_sendstr(req, "INTERIOR_LIGHTS_OFF\n");
    return ESP_OK;
}

// Climate
static esp_err_t http_ac_on_handler(httpd_req_t *req)
{
    duocan_cmd_ac_on();
    httpd_resp_sendstr(req, "AC_ON\n");
    return ESP_OK;
}

static esp_err_t http_ac_off_handler(httpd_req_t *req)
{
    duocan_cmd_ac_off();
    httpd_resp_sendstr(req, "AC_OFF\n");
    return ESP_OK;
}

static esp_err_t http_fan_up_handler(httpd_req_t *req)
{
    duocan_cmd_fan_up();
    httpd_resp_sendstr(req, "FAN_UP\n");
    return ESP_OK;
}

static esp_err_t http_fan_down_handler(httpd_req_t *req)
{
    duocan_cmd_fan_down();
    httpd_resp_sendstr(req, "FAN_DOWN\n");
    return ESP_OK;
}

// Audio
static esp_err_t http_audio_mute_handler(httpd_req_t *req)
{
    duocan_cmd_audio_mute();
    httpd_resp_sendstr(req, "AUDIO_MUTE\n");
    return ESP_OK;
}

static esp_err_t http_audio_unmute_handler(httpd_req_t *req)
{
    duocan_cmd_audio_unmute();
    httpd_resp_sendstr(req, "AUDIO_UNMUTE\n");
    return ESP_OK;
}

// Trunk
static esp_err_t http_trunk_open_handler(httpd_req_t *req)
{
    duocan_cmd_trunk_open();
    httpd_resp_sendstr(req, "TRUNK_OPEN\n");
    return ESP_OK;
}

// Horn
static esp_err_t http_horn_handler(httpd_req_t *req)
{
    duocan_cmd_horn();
    httpd_resp_sendstr(req, "HORN\n");
    return ESP_OK;
}

// Hazards
static esp_err_t http_hazards_on_handler(httpd_req_t *req)
{
    duocan_cmd_hazards_on();
    httpd_resp_sendstr(req, "HAZARDS_ON\n");
    return ESP_OK;
}

static esp_err_t http_hazards_off_handler(httpd_req_t *req)
{
    duocan_cmd_hazards_off();
    httpd_resp_sendstr(req, "HAZARDS_OFF\n");
    return ESP_OK;
}

// Defrost
static esp_err_t http_defrost_on_handler(httpd_req_t *req)
{
    duocan_cmd_defrost_on();
    httpd_resp_sendstr(req, "DEFROST_ON\n");
    return ESP_OK;
}

static esp_err_t http_defrost_off_handler(httpd_req_t *req)
{
    duocan_cmd_defrost_off();
    httpd_resp_sendstr(req, "DEFROST_OFF\n");
    return ESP_OK;
}

// ------------------------------------------------------------
// START HTTP SERVER
// ------------------------------------------------------------
static httpd_handle_t start_http_server(void)
{
    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port   = 32768;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return NULL;
    }

    // Core endpoints
    httpd_register_uri_handler(server, &(httpd_uri_t){"/ping", HTTP_GET, http_ping_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/status", HTTP_GET, http_status_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/enable_can", HTTP_GET, http_enable_can_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/disable_can", HTTP_GET, http_disable_can_handler, NULL});

    // Doors
    httpd_register_uri_handler(server, &(httpd_uri_t){"/lock", HTTP_GET, http_lock_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/unlock", HTTP_GET, http_unlock_handler, NULL});

    // Windows
    httpd_register_uri_handler(server, &(httpd_uri_t){"/windows_down", HTTP_GET, http_windows_down_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/windows_up", HTTP_GET, http_windows_up_handler, NULL});

    // Sunroof
    httpd_register_uri_handler(server, &(httpd_uri_t){"/sunroof_open", HTTP_GET, http_sunroof_open_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/sunroof_close", HTTP_GET, http_sunroof_close_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/sunroof_vent", HTTP_GET, http_sunroof_vent_handler, NULL});

    // Lights
    httpd_register_uri_handler(server, &(httpd_uri_t){"/headlights_on", HTTP_GET, http_headlights_on_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/headlights_off", HTTP_GET, http_headlights_off_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/fog_on", HTTP_GET, http_fog_on_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/fog_off", HTTP_GET, http_fog_off_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/interior_on", HTTP_GET, http_interior_on_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/interior_off", HTTP_GET, http_interior_off_handler, NULL});

    // Climate
    httpd_register_uri_handler(server, &(httpd_uri_t){"/ac_on", HTTP_GET, http_ac_on_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/ac_off", HTTP_GET, http_ac_off_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/fan_up", HTTP_GET, http_fan_up_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/fan_down", HTTP_GET, http_fan_down_handler, NULL});

    // Audio
    httpd_register_uri_handler(server, &(httpd_uri_t){"/audio_mute", HTTP_GET, http_audio_mute_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/audio_unmute", HTTP_GET, http_audio_unmute_handler, NULL});

    // Trunk
    httpd_register_uri_handler(server, &(httpd_uri_t){"/trunk_open", HTTP_GET, http_trunk_open_handler, NULL});

    // Horn
    httpd_register_uri_handler(server, &(httpd_uri_t){"/horn", HTTP_GET, http_horn_handler, NULL});

    // Hazards
    httpd_register_uri_handler(server, &(httpd_uri_t){"/hazards_on", HTTP_GET, http_hazards_on_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/hazards_off", HTTP_GET, http_hazards_off_handler, NULL});

    // Defrost
    httpd_register_uri_handler(server, &(httpd_uri_t){"/defrost_on", HTTP_GET, http_defrost_on_handler, NULL});
    httpd_register_uri_handler(server, &(httpd_uri_t){"/defrost_off", HTTP_GET, http_defrost_off_handler, NULL});

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return server;
}

// ------------------------------------------------------------
// APP MAIN
// ------------------------------------------------------------
void app_main(void)
{
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

    httpd_handle_t http_server = start_http_server();
    if (http_server == NULL) {
        ESP_LOGE(TAG, "HTTP server start FAILED");
    }

    BaseType_t tcp_task_ret = xTaskCreate(
        tcp_server_task,
        "tcp_server",
        4096,
        NULL,
        10,
        NULL
    );

    if (tcp_task_ret != pdPASS) {
        ESP_LOGE(TAG, "TCP server task creation FAILED");
        led1_set_red();
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "DuoCAN ready (Wi-Fi AP + CAN + TCP Queue + TCP Server + HTTP Server)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
