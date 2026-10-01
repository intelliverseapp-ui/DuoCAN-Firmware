#include "ws2812.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include <string.h>

//
// DuoCAN Rev A WS2812 LED Driver
// GPIO18 (D10) → WS2812 DIN
// 2 LEDs total
// RMT @ 20 MHz (50 ns per tick)
// WS2812 timing:
//   T0H = 350 ns → 7 ticks
//   T0L = 800 ns → 16 ticks
//   T1H = 700 ns → 14 ticks
//   T1L = 600 ns → 12 ticks
//

#define LED_PIN        18
#define NUM_LEDS       2
#define RMT_RES_HZ     20000000  // 20 MHz

static const char *TAG = "WS2812";

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

typedef struct {
    uint8_t g, r, b;   // WS2812 uses GRB order
} __attribute__((packed)) ws2812_pixel_t;

static ws2812_pixel_t pixels[NUM_LEDS];

void ws2812_init(void)
{
    ESP_LOGI(TAG, "Initializing WS2812 on GPIO %d (D10) with 20MHz RMT...", LED_PIN);

    // -----------------------------
    // RMT TX Channel Configuration
    // -----------------------------
    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num = LED_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_RES_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &led_chan));

    // -----------------------------
    // WS2812 Bit Encoder
    // Durations are in RMT ticks
    // -----------------------------
    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = { .duration0 = 7,  .level0 = 1, .duration1 = 16, .level1 = 0 }, // 0-bit
        .bit1 = { .duration0 = 14, .level0 = 1, .duration1 = 12, .level1 = 0 }, // 1-bit
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &led_encoder));

    ESP_ERROR_CHECK(rmt_enable(led_chan));

    // *** IMPORTANT CHANGE ***
    // We no longer clear LEDs here.
    // Application code (duocan_leds.c) controls LED state.
    // Removing ws2812_clear() and ws2812_show() prevents wiping LED1 GREEN.

    ESP_LOGI(TAG, "WS2812 driver ready");
}

static void ws2812_refresh(void)
{
    if (!led_chan || !led_encoder) {
        ESP_LOGE(TAG, "WS2812 refresh called before init");
        return;
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0
    };

    esp_err_t err = rmt_transmit(
        led_chan,
        led_encoder,
        pixels,
        sizeof(pixels),
        &tx_cfg
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 data transmit FAILED: %s", esp_err_to_name(err));
        return;
    }

    err = rmt_tx_wait_all_done(led_chan, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 wait FAILED: %s", esp_err_to_name(err));
        return;
    }
}

//
// Legacy API: whole-strip color
//
void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        pixels[i].g = g;
        pixels[i].r = r;
        pixels[i].b = b;
    }
    ws2812_refresh();
}

//
// Legacy API: dual LED color
//
void ws2812_set_dual(uint8_t r1, uint8_t g1, uint8_t b1,
                     uint8_t r2, uint8_t g2, uint8_t b2)
{
    pixels[0].g = g1;
    pixels[0].r = r1;
    pixels[0].b = b1;

    pixels[1].g = g2;
    pixels[1].r = r2;
    pixels[1].b = b2;

    ws2812_refresh();
}

//
// Modern API: per-pixel control
//
void ws2812_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= NUM_LEDS) return;

    pixels[index].g = g;
    pixels[index].r = r;
    pixels[index].b = b;
}

void ws2812_show(void)
{
    ws2812_refresh();
}

void ws2812_clear(void)
{
    memset(pixels, 0, sizeof(pixels));
}
