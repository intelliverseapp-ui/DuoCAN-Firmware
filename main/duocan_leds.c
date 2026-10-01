#include "duocan_leds.h"
#include "ws2812.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

//
// DuoCAN Rev A LED subsystem
// Uses WS2812 addressable RGB LEDs on GPIO18 (D10)
// LED1 = System Status  (pixel 0)
// LED2 = CAN Activity   (pixel 1)
//

static const char *TAG = "DuoCAN_LEDS";

// Pixel indices
#define LED1_INDEX 0
#define LED2_INDEX 1

// Mutex to serialize WS2812 updates
static SemaphoreHandle_t led_mutex = NULL;

// ------------------------------------------------------------
// Internal safe update wrapper (ONLY this function owns mutex)
// ------------------------------------------------------------
static void safe_ws2812_update(void)
{
    if (led_mutex) {
        xSemaphoreTake(led_mutex, portMAX_DELAY);
    }

    ws2812_show();

    if (led_mutex) {
        xSemaphoreGive(led_mutex);
    }
}

// ------------------------------------------------------------
// Initialization
// ------------------------------------------------------------
void duocan_leds_init(void)
{
    ESP_LOGI(TAG, "Initializing DuoCAN WS2812 LED subsystem...");

    led_mutex = xSemaphoreCreateMutex();
    if (!led_mutex) {
        ESP_LOGE(TAG, "LED mutex creation FAILED");
    }

    ws2812_init();

    ESP_LOGI(TAG, "DuoCAN LEDs ready");
}

// ------------------------------------------------------------
// Legacy convenience API (kept for compatibility)
// ------------------------------------------------------------
void led1_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_set_pixel(LED1_INDEX, r, g, b);
    safe_ws2812_update();
}

void led1_set_red(void)   { led1_set_rgb(255, 0, 0); }
void led1_set_green(void) { led1_set_rgb(0, 255, 0); }
void led1_set_blue(void)  { led1_set_rgb(0, 0, 255); }
void led1_set_off(void)   { led1_set_rgb(0, 0, 0); }

void led2_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_set_pixel(LED2_INDEX, r, g, b);
    safe_ws2812_update();
}

void led2_set_red(void)   { led2_set_rgb(255, 0, 0); }
void led2_set_green(void) { led2_set_rgb(0, 255, 0); }
void led2_set_blue(void)  { led2_set_rgb(0, 0, 255); }
void led2_set_off(void)   { led2_set_rgb(0, 0, 0); }

// ------------------------------------------------------------
// Automotive Status API
// ------------------------------------------------------------

// CAN idle = dim white
void duocan_leds_can_idle(void)
{
    led2_set_rgb(10, 10, 10);
}

// CAN RX = green
void duocan_leds_can_rx_active(void)
{
    led2_set_rgb(0, 255, 0);
}

// CAN TX = blue
void duocan_leds_can_tx_active(void)
{
    led2_set_rgb(0, 0, 255);
}

// Wi-Fi AP down = LED1 off
void duocan_leds_wifi_ap_down(void)
{
    led1_set_off();
}

// Wi-Fi AP up = cyan
void duocan_leds_wifi_ap_up(void)
{
    led1_set_rgb(0, 255, 255);
}

// TCP server down = yellow
void duocan_leds_tcp_server_down(void)
{
    led1_set_rgb(255, 255, 0);
}

// TCP server up = magenta
void duocan_leds_tcp_server_up(void)
{
    led1_set_rgb(255, 0, 255);
}

// Error = both LEDs solid red
void duocan_leds_error(void)
{
    ws2812_set_pixel(LED1_INDEX, 255, 0, 0);
    ws2812_set_pixel(LED2_INDEX, 255, 0, 0);
    safe_ws2812_update();
}

// Clear both LEDs
void duocan_leds_clear_all(void)
{
    ws2812_clear();
    safe_ws2812_update();
}
