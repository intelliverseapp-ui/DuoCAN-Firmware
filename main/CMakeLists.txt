idf_component_register(
    SRCS
        "main.c"
        "duocan_leds.c"
        "ws2812.c"
        "tcp_server.c"
        "duocan_can.c"
        "tcp_queue.c"
        "led_test_harness.c"
        "duocan_commands.c"

    INCLUDE_DIRS
        "."

    PRIV_REQUIRES
        driver
        nvs_flash
        esp_wifi
        esp_netif
        esp_http_server
        freertos
        lwip
)
