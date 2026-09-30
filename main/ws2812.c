#include "ws2812.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

#define LED_PIN 21
#define NUM_LEDS 2

static const char *TAG = "WS2812";

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

typedef struct {
    uint8_t g, r, b;
} __attribute__((packed)) ws2812_pixel_t;

static ws2812_pixel_t pixels[NUM_LEDS];

void ws2812_init(void)
{
    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num = LED_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .mem_block_symbols = 64,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &led_chan));

    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = { .duration0 = 350, .level0 = 1, .duration1 = 800, .level1 = 0 },
        .bit1 = { .duration0 = 700, .level0 = 1, .duration1 = 600, .level1 = 0 },
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &led_encoder));

    ESP_ERROR_CHECK(rmt_enable(led_chan));

    ESP_LOGI(TAG, "WS2812 initialized");
}

static void ws2812_refresh(void)
{
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0
    };

    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder,
                                 pixels, sizeof(pixels), &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, -1));
}

void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        pixels[i].r = r;
        pixels[i].g = g;
        pixels[i].b = b;
    }
    ws2812_refresh();
}

void ws2812_set_dual(uint8_t r1, uint8_t g1, uint8_t b1,
                     uint8_t r2, uint8_t g2, uint8_t b2)
{
    pixels[0].r = r1;
    pixels[0].g = g1;
    pixels[0].b = b1;

    pixels[1].r = r2;
    pixels[1].g = g2;
    pixels[1].b = b2;

    ws2812_refresh();
}
