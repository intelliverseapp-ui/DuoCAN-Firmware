#include "duocan_leds.h"
#include "ws2812.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_TEST";

// Helper delay
static void wait(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
}

// Boot animation
static void boot_animation(void)
{
    ESP_LOGI(TAG, "Running boot animation...");

    // Pixel 0 sweep
    ws2812_set_pixel(0, 255, 0, 0); ws2812_show(); vTaskDelay(pdMS_TO_TICKS(200));
    ws2812_set_pixel(0, 0, 255, 0); ws2812_show(); vTaskDelay(pdMS_TO_TICKS(200));
    ws2812_set_pixel(0, 0, 0, 255); ws2812_show(); vTaskDelay(pdMS_TO_TICKS(200));

    // Pixel 1 sweep
    ws2812_set_pixel(1, 255, 0, 0); ws2812_show(); vTaskDelay(pdMS_TO_TICKS(200));
    ws2812_set_pixel(1, 0, 255, 0); ws2812_show(); vTaskDelay(pdMS_TO_TICKS(200));
    ws2812_set_pixel(1, 0, 0, 255); ws2812_show(); vTaskDelay(pdMS_TO_TICKS(200));

    ws2812_clear();
    ws2812_show();
}

void led_test_task(void *arg)
{
    ESP_LOGI(TAG, "Starting sequential LED test harness...");

    duocan_leds_init();
    boot_animation();

    while (true)
    {
        ESP_LOGI(TAG, "Testing CAN idle...");
        duocan_leds_can_idle();
        wait();

        ESP_LOGI(TAG, "Testing CAN RX active...");
        duocan_leds_can_rx_active();
        wait();

        ESP_LOGI(TAG, "Testing CAN TX active...");
        duocan_leds_can_tx_active();
        wait();

        ESP_LOGI(TAG, "Testing Wi-Fi AP down...");
        duocan_leds_wifi_ap_down();
        wait();

        ESP_LOGI(TAG, "Testing Wi-Fi AP up...");
        duocan_leds_wifi_ap_up();
        wait();

        ESP_LOGI(TAG, "Testing TCP server down...");
        duocan_leds_tcp_server_down();
        wait();

        ESP_LOGI(TAG, "Testing TCP server up...");
        duocan_leds_tcp_server_up();
        wait();

        ESP_LOGI(TAG, "Testing error mode...");
        duocan_leds_error();
        wait();

        ESP_LOGI(TAG, "Testing clear all...");
        duocan_leds_clear_all();
        wait();

        // Legacy LED1/LED2 tests
        ESP_LOGI(TAG, "Testing legacy LED1 red...");
        led1_set_red();
        wait();

        ESP_LOGI(TAG, "Testing legacy LED1 green...");
        led1_set_green();
        wait();

        ESP_LOGI(TAG, "Testing legacy LED1 blue...");
        led1_set_blue();
        wait();

        ESP_LOGI(TAG, "Testing legacy LED1 off...");
        led1_set_off();
        wait();

        ESP_LOGI(TAG, "Testing legacy LED2 red...");
        led2_set_red();
        wait();

        ESP_LOGI(TAG, "Testing legacy LED2 green...");
        led2_set_green();
        wait();

        ESP_LOGI(TAG, "Testing legacy LED2 blue...");
        led2_set_blue();
        wait();

        ESP_LOGI(TAG, "Testing legacy LED2 off...");
        led2_set_off();
        wait();

        // Per-pixel WS2812 tests
        ESP_LOGI(TAG, "Testing per-pixel WS2812 API...");
        ws2812_set_pixel(0, 255, 128, 0); // amber
        ws2812_set_pixel(1, 0, 128, 255); // light blue
        ws2812_show();
        wait();

        ESP_LOGI(TAG, "Testing WS2812 clear...");
        ws2812_clear();
        ws2812_show();
        wait();
    }
}

void app_main(void)
{
    xTaskCreate(led_test_task, "led_test_task", 4096, NULL, 5, NULL);
}
