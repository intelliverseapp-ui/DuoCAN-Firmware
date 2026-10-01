#include "tcp_server.h"
#include "tcp_queue.h"
#include "duocan_leds.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "TCP";

#define TCP_PORT 1234
#define RX_BUF_SIZE 256
#define CMD_BUF_SIZE 512

// Global TCP client socket for CAN RX forwarding
int g_tcp_client_sock = -1;

esp_err_t duocan_enable_can(void);
esp_err_t duocan_disable_can(void);
esp_err_t duocan_send_can_frame(uint32_t id, uint8_t dlc, const uint8_t *data);
void duocan_get_status(char *out, size_t out_len);

// ------------------------------------------------------------
// Safe socket send (handles partial writes)
// ------------------------------------------------------------
int safe_send(int sock, const char *data, size_t len)
{
    size_t total = 0;

    while (total < len) {
        int sent = send(sock, data + total, len - total, 0);

        if (sent < 0) {
            ESP_LOGE(TAG, "Socket send error");
            return -1;
        }

        total += sent;
    }

    return (int)total;
}

// ------------------------------------------------------------
// CAN RX forwarding now uses outbound queue
// ------------------------------------------------------------
void tcp_server_send_line(const char *line)
{
    // DO NOT send directly.
    // Queue it for the TCP sender task.
    tcp_queue_push(line);
}

// ------------------------------------------------------------
// Command parser
// ------------------------------------------------------------
static void handle_command(const char *cmd, int client_sock)
{
    char response[256];

    ESP_LOGI(TAG, "CMD: %s", cmd);

    if (strcasecmp(cmd, "PING") == 0) {
        safe_send(client_sock, "PONG\n", 5);
        return;
    }

    if (strcasecmp(cmd, "ENABLE_CAN") == 0) {
        duocan_enable_can();
        safe_send(client_sock, "CAN ENABLED\n", 12);
        return;
    }

    if (strcasecmp(cmd, "DISABLE_CAN") == 0) {
        duocan_disable_can();
        safe_send(client_sock, "CAN DISABLED\n", 13);
        return;
    }

    if (strcasecmp(cmd, "STATUS") == 0) {
        duocan_get_status(response, sizeof(response));
        safe_send(client_sock, response, strlen(response));
        return;
    }

    if (strncasecmp(cmd, "SEND ", 5) == 0) {

        uint32_t id = 0;
        uint32_t dlc = 0;
        uint32_t bytes[8] = {0};

        int count = sscanf(cmd + 5,
                           "%" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32,
                           &id, &dlc,
                           &bytes[0], &bytes[1], &bytes[2], &bytes[3],
                           &bytes[4], &bytes[5], &bytes[6], &bytes[7]);

        if (count < 2) {
            safe_send(client_sock, "ERR BAD SEND FORMAT\n", 20);
            return;
        }

        if (dlc > 8) {
            safe_send(client_sock, "ERR DLC > 8\n", 12);
            return;
        }

        uint8_t data[8];
        for (int i = 0; i < dlc; i++) {
            data[i] = (uint8_t)bytes[i];
        }

        duocan_send_can_frame(id, dlc, data);

        snprintf(response, sizeof(response),
                 "SENT ID=%" PRIu32 " DLC=%" PRIu32 "\n", id, dlc);
        safe_send(client_sock, response, strlen(response));
        return;
    }

    snprintf(response, sizeof(response),
             "ERR UNKNOWN CMD: %.200s\n", cmd);
    safe_send(client_sock, response, strlen(response));
}

// ------------------------------------------------------------
// TCP Server Task
// ------------------------------------------------------------
void tcp_server_task(void *arg)
{
    char rx_buffer[RX_BUF_SIZE];
    char cmd_buffer[CMD_BUF_SIZE];
    size_t cmd_len = 0;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) < 0) {
        ESP_LOGE(TAG, "Socket listen failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    while (1) {
        struct sockaddr_in6 client_addr;
        socklen_t addr_len = sizeof(client_addr);

        g_tcp_client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
        int client_sock = g_tcp_client_sock;

        if (client_sock < 0) {
            ESP_LOGE(TAG, "Accept failed");
            continue;
        }

        ESP_LOGI(TAG, "Client connected");
        duocan_leds_tcp_server_up();   // Magenta LED

        safe_send(client_sock, "DuoCAN TCP READY\n", 17);

        cmd_len = 0;

        while (1) {
            int len = recv(client_sock, rx_buffer, RX_BUF_SIZE - 1, 0);

            if (len <= 0) {
                ESP_LOGI(TAG, "Client disconnected");
                duocan_leds_tcp_server_down();   // Yellow LED
                break;
            }

            rx_buffer[len] = 0;

            for (int i = 0; i < len; i++) {
                char c = rx_buffer[i];

                if (c == '\n') {
                    cmd_buffer[cmd_len] = 0;
                    handle_command(cmd_buffer, client_sock);
                    cmd_len = 0;
                }
                else if (cmd_len < CMD_BUF_SIZE - 1) {
                    cmd_buffer[cmd_len++] = c;
                }
                else {
                    safe_send(client_sock, "ERR CMD TOO LONG\n", 17);
                    cmd_len = 0;
                }
            }
        }

        close(client_sock);
        g_tcp_client_sock = -1;
    }

    close(listen_sock);
    vTaskDelete(NULL);
}
