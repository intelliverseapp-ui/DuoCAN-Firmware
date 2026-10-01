#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// CAN subsystem initialization
esp_err_t duocan_can_init(void);

// CAN control
esp_err_t duocan_enable_can(void);
esp_err_t duocan_disable_can(void);

// CAN TX
esp_err_t duocan_send_can_frame(uint32_t id, uint8_t dlc, const uint8_t *data);

// CAN status
void duocan_get_status(char *out, size_t out_len);

// CAN RX forwarding task (required by main.c)
void duocan_can_rx_forward_task(void *arg);
