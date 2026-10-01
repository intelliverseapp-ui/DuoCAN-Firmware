#include "duocan_leds.h"
#include "ws2812.h"
#include "esp_log.h"

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

// ------------------------------------------------------------
// Initialization
// ------------------------------------------------------------
void duocan_leds_init(void)
{
    ESP_LOGI(TAG, "Initializing DuoCAN WS2812 LED subsystem...");
    ws2812_init();
    ws2812_clear();
    ws2812_show();
    ESP_LOGI(TAG, "DuoCAN LEDs ready");
}

// ------------------------------------------------------------
// Legacy convenience API (kept for compatibility)
// ------------------------------------------------------------
void led1_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_set_pixel(LED1_INDEX, r, g, b);
    ws2812_show();
}

void led1_set_red(void)   { led1_set_rgb(255, 0, 0); }
void led1_set_green(void) { led1_set_rgb(0, 255, 0); }
void led1_set_blue(void)  { led1_set_rgb(0, 0, 255); }
void led1_set_off(void)   { led1_set_rgb(0, 0, 0); }

void led2_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_set_pixel(LED2_INDEX, r, g, b);
    ws2812_show();
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
    ws2812_set_pixel(LED2_INDEX, 10, 10, 10);
    ws2812_show();
}

// CAN RX = green
void duocan_leds_can_rx_active(void)
{
    ws2812_set_pixel(LED2_INDEX, 0, 255, 0);
    ws2812_show();
}

// CAN TX = blue
void duocan_leds_can_tx_active(void)
{
    ws2812_set_pixel(LED2_INDEX, 0, 0, 255);
    ws2812_show();
}

// Wi-Fi AP down = LED1 off
void duocan_leds_wifi_ap_down(void)
{
    ws2812_set_pixel(LED1_INDEX, 0, 0, 0);
    ws2812_show();
}

// Wi-Fi AP up = cyan
void duocan_leds_wifi_ap_up(void)
{
    ws2812_set_pixel(LED1_INDEX, 0, 255, 255);
    ws2812_show();
}

// TCP server down = yellow
void duocan_leds_tcp_server_down(void)
{
    ws2812_set_pixel(LED1_INDEX, 255, 255, 0);
    ws2812_show();
}

// TCP server up = magenta
void duocan_leds_tcp_server_up(void)
{
    ws2812_set_pixel(LED1_INDEX, 255, 0, 255);
    ws2812_show();
}

// Error = both LEDs solid red
void duocan_leds_error(void)
{
    ws2812_set_pixel(LED1_INDEX, 255, 0, 0);
    ws2812_set_pixel(LED2_INDEX, 255, 0, 0);
    ws2812_show();
}

// Clear both LEDs
void duocan_leds_clear_all(void)
{
    ws2812_clear();
    ws2812_show();
}
