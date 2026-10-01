#pragma once
#include <stdint.h>
#include <stdbool.h>

// Initialize DuoCAN LED subsystem (WS2812 on GPIO18 / D10)
void duocan_leds_init(void);

// ------------------------------------------------------------
// Legacy convenience API (kept for compatibility)
// These now map to WS2812 pixel 0 (LED1) and pixel 1 (LED2)
// ------------------------------------------------------------

// System Status LED (LED1 → pixel 0)
void led1_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void led1_set_red(void);
void led1_set_green(void);
void led1_set_blue(void);
void led1_set_off(void);

// CAN Activity LED (LED2 → pixel 1)
void led2_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void led2_set_red(void);
void led2_set_green(void);
void led2_set_blue(void);
void led2_set_off(void);

// ------------------------------------------------------------
// Automotive Status API (new)
// ------------------------------------------------------------

// CAN bus status
void duocan_leds_can_idle(void);
void duocan_leds_can_rx_active(void);
void duocan_leds_can_tx_active(void);

// Wi-Fi AP status
void duocan_leds_wifi_ap_down(void);
void duocan_leds_wifi_ap_up(void);

// TCP server status
void duocan_leds_tcp_server_down(void);
void duocan_leds_tcp_server_up(void);

// Error indicator
void duocan_leds_error(void);

// Clear both LEDs
void duocan_leds_clear_all(void);
