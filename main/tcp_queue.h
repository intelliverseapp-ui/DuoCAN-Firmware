#pragma once
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// DuoCAN TCP Outbound Queue
// ---------------------------------------------
// Decouples CAN RX timing from TCP send timing.
// CAN RX pushes lines into this queue.
// A dedicated task pops items and sends them
// through tcp_server_send_line_safe().
// ---------------------------------------------

// Maximum length of a single outbound line
#define TCP_QUEUE_MAX_LINE_LEN 256

// Number of queued outbound messages
#define TCP_QUEUE_LENGTH 64

// Queue item structure
typedef struct {
    char line[TCP_QUEUE_MAX_LINE_LEN];
    size_t len;
} tcp_queue_item_t;

// Global queue handle
extern QueueHandle_t tcp_outbound_queue;

// Initialize queue + start sender task
void tcp_queue_init(void);

// Push a line into the outbound queue (non-blocking)
void tcp_queue_push(const char *line);

// Internal task that drains the queue and sends lines
void tcp_queue_task(void *arg);
