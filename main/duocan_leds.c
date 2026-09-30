#include "duocan_leds.h"
#include "driver/gpio.h"

// DuoCAN Rev A LED pin map

// LED1 (System Status)
#define LED1_R 10
#define LED1_G 9
#define LED1_B 8

// LED2 (CAN Activity)
#define LED2_R 14
#define LED2_G 13
#define LED2_B 12

static void configure_pin(int pin)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
}

void duocan_leds_init(void)
{
    // Configure all LED pins
    configure_pin(LED1_R);
    configure_pin(LED1_G);
    configure_pin(LED1_B);

    configure_pin(LED2_R);
    configure_pin(LED2_G);
    configure_pin(LED2_B);
}

// ---------------- LED1 (System Status) ----------------

void led1_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    gpio_set_level(LED1_R, r ? 1 : 0);
    gpio_set_level(LED1_G, g ? 1 : 0);
    gpio_set_level(LED1_B, b ? 1 : 0);
}

void led1_set_red(void)   { led1_set_rgb(255, 0, 0); }
void led1_set_green(void) { led1_set_rgb(0, 255, 0); }
void led1_set_blue(void)  { led1_set_rgb(0, 0, 255); }
void led1_set_off(void)   { led1_set_rgb(0, 0, 0); }

// ---------------- LED2 (CAN Activity) ----------------

void led2_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    gpio_set_level(LED2_R, r ? 1 : 0);
    gpio_set_level(LED2_G, g ? 1 : 0);
    gpio_set_level(LED2_B, b ? 1 : 0);
}

void led2_set_red(void)   { led2_set_rgb(255, 0, 0); }
void led2_set_green(void) { led2_set_rgb(0, 255, 0); }
void led2_set_blue(void)  { led2_set_rgb(0, 0, 255); }
void led2_set_off(void)   { led2_set_rgb(0, 0, 0); }
