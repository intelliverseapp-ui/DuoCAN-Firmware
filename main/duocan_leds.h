#pragma once
#include <stdint.h>
#include <stdbool.h>

void duocan_leds_init(void);

// System status LED (LED1)
void led1_set_rgb(uint8_t r, uint8_t g, uint8_t b);

// CAN activity LED (LED2)
void led2_set_rgb(uint8_t r, uint8_t g, uint8_t b);

// Convenience helpers
void led1_set_red(void);
void led1_set_green(void);
void led1_set_blue(void);
void led1_set_off(void);

void led2_set_red(void);
void led2_set_green(void);
void led2_set_blue(void);
void led2_set_off(void);
