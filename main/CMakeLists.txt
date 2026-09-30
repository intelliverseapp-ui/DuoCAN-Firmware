idf_component_register(
    SRCS
        "main.c"
        "duocan_leds.c"
        "tcp_server.c"
        "duocan_can.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        driver
        nvs_flash
        esp_wifi
        esp_netif
)
