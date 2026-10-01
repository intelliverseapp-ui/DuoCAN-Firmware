#include <unistd.h>            // for close()

// IMPORTANT: tcp_server.h MUST come BEFORE tcp_queue.h
#include "tcp_server.h"        // for g_tcp_client_sock and safe_send()

#include "tcp_queue.h"
#include "duocan_leds.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "TCP_QUEUE";

// Global queue handle
QueueHandle_t tcp_outbound_queue = NULL;

// ------------------------------------------------------------
// Push a line into the outbound queue (non-blocking)
// ------------------------------------------------------------
void tcp_queue_push(const char *line)
{
    if (!tcp_outbound_queue || !line) {
        ESP_LOGE(TAG, "Queue not ready or line NULL");
        return;
    }

    tcp_queue_item_t item;
    memset(&item, 0, sizeof(item));

    // Copy safely
    size_t len = strnlen(line, TCP_QUEUE_MAX_LINE_LEN - 1);
    memcpy(item.line, line, len);
    item.len = len;

    BaseType_t ok = xQueueSend(tcp_outbound_queue, &item, 0);

    if (ok != pdTRUE) {
        ESP_LOGW(TAG, "TCP outbound queue FULL — dropping line");
        duocan_leds_tcp_server_down();   // Yellow LED = queue overflow
    }
}

// ------------------------------------------------------------
// Queue consumer task — drains queue and sends lines to TCP client
// ------------------------------------------------------------
void tcp_queue_task(void *arg)
{
    tcp_queue_item_t item;

    ESP_LOGI(TAG, "TCP queue sender task started");

    while (1) {
        // Wait forever for next item
        if (xQueueReceive(tcp_outbound_queue, &item, portMAX_DELAY) == pdTRUE) {

            // Queue has data → TCP server active
            duocan_leds_tcp_server_up();   // Magenta LED

            // If no client is connected, drop the frame
            if (g_tcp_client_sock < 0) {
                ESP_LOGW(TAG, "No TCP client connected — dropping telemetry");
                continue;
            }

            // Send the line directly to the TCP client
            int sent = safe_send(g_tcp_client_sock, item.line, item.len);

            if (sent < 0) {
                ESP_LOGE(TAG, "TCP send failed — closing client socket");
                close(g_tcp_client_sock);
                g_tcp_client_sock = -1;
                duocan_leds_tcp_server_down();   // Yellow LED = connection down
            }

            // If queue becomes empty → stable active state
            if (uxQueueMessagesWaiting(tcp_outbound_queue) == 0) {
                duocan_leds_tcp_server_up();   // Magenta = active but stable
            }
        }
    }
}

// ------------------------------------------------------------
// Initialization — create queue + start sender task
// ------------------------------------------------------------
void tcp_queue_init(void)
{
    ESP_LOGI(TAG, "Initializing TCP outbound queue...");

    tcp_outbound_queue = xQueueCreate(TCP_QUEUE_LENGTH, sizeof(tcp_queue_item_t));

    if (!tcp_outbound_queue) {
        ESP_LOGE(TAG, "FAILED to create TCP outbound queue");
        duocan_leds_error();
        return;
    }

    // Create sender task
    BaseType_t ok = xTaskCreate(
        tcp_queue_task,
        "tcp_queue_task",
        4096,
        NULL,
        5,
        NULL
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "FAILED to start TCP queue task");
        duocan_leds_error();
        return;
    }

    ESP_LOGI(TAG, "TCP outbound queue ready");
}
