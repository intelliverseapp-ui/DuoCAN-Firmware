// main/ws2812.h
#pragma once

#include <stdint.h>

// Initialize WS2812 driver (GPIO18 / D10, 2 LEDs, RMT @ 20 MHz)
void ws2812_init(void);

// Legacy whole-strip APIs (kept for compatibility)
void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b);
void ws2812_set_dual(uint8_t r1, uint8_t g1, uint8_t b1,
                     uint8_t r2, uint8_t g2, uint8_t b2);

// Modern per-pixel APIs
void ws2812_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void ws2812_show(void);
void ws2812_clear(void);
