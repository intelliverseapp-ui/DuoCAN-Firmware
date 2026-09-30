#pragma once
#include <stdint.h>
#include <stddef.h>

// CAN subsystem initialization
void duocan_can_init(void);

// CAN control
void duocan_enable_can(void);
void duocan_disable_can(void);

// CAN TX
void duocan_send_can_frame(uint32_t id, uint8_t dlc, uint8_t *data);

// CAN status
void duocan_get_status(char *out, size_t out_len);

// CAN RX forwarding task (required by main.c)
void duocan_can_rx_forward_task(void *arg);
