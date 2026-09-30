#include "tcp_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "TCP";

#define TCP_PORT 1234
#define RX_BUF_SIZE 256

// Global TCP client socket for CAN RX forwarding
int g_tcp_client_sock = -1;

void duocan_enable_can(void);
void duocan_disable_can(void);
void duocan_send_can_frame(uint32_t id, uint8_t dlc, uint8_t *data);
void duocan_get_status(char *out, size_t out_len);

// ------------------------------------------------------------
// CAN RX forwarding function (called from duocan_can.c)
// ------------------------------------------------------------
void tcp_server_send_line(const char *line)
{
    if (g_tcp_client_sock > 0) {
        send(g_tcp_client_sock, line, strlen(line), 0);
    }
}

// ------------------------------------------------------------
// Command parser
// ------------------------------------------------------------
static void handle_command(const char *cmd, int client_sock)
{
    char response[256];

    char clean[RX_BUF_SIZE];
    snprintf(clean, sizeof(clean), "%s", cmd);
    clean[strcspn(clean, "\r\n")] = 0;

    ESP_LOGI(TAG, "CMD: %s", clean);

    if (strcasecmp(clean, "PING") == 0) {
        send(client_sock, "PONG\n", 5, 0);
        return;
    }

    if (strcasecmp(clean, "ENABLE_CAN") == 0) {
        duocan_enable_can();
        send(client_sock, "CAN ENABLED\n", 12, 0);
        return;
    }

    if (strcasecmp(clean, "DISABLE_CAN") == 0) {
        duocan_disable_can();
        send(client_sock, "CAN DISABLED\n", 13, 0);
        return;
    }

    if (strcasecmp(clean, "STATUS") == 0) {
        duocan_get_status(response, sizeof(response));
        send(client_sock, response, strlen(response), 0);
        return;
    }

    if (strncasecmp(clean, "SEND ", 5) == 0) {

        uint32_t id = 0;
        uint32_t dlc = 0;
        uint32_t bytes[8] = {0};

        int count = sscanf(clean + 5,
                           "%" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32 " %" SCNu32,
                           &id, &dlc,
                           &bytes[0], &bytes[1], &bytes[2], &bytes[3],
                           &bytes[4], &bytes[5], &bytes[6], &bytes[7]);

        if (count < 2) {
            send(client_sock, "ERR BAD SEND FORMAT\n", 20, 0);
            return;
        }

        if (dlc > 8) {
            send(client_sock, "ERR DLC > 8\n", 12, 0);
            return;
        }

        uint8_t data[8];
        for (int i = 0; i < dlc; i++) {
            data[i] = (uint8_t)bytes[i];
        }

        duocan_send_can_frame(id, dlc, data);

        snprintf(response, sizeof(response),
                 "SENT ID=%" PRIu32 " DLC=%" PRIu32 "\n", id, dlc);
        send(client_sock, response, strlen(response), 0);
        return;
    }

    snprintf(response, sizeof(response),
             "ERR UNKNOWN CMD: %.200s\n", clean);
    send(client_sock, response, strlen(response), 0);
}

// ------------------------------------------------------------
// TCP Server Task
// ------------------------------------------------------------
void tcp_server_task(void *arg)
{
    char rx_buffer[RX_BUF_SIZE];

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

        const char *hello = "DuoCAN TCP READY\n";
        send(client_sock, hello, strlen(hello), 0);

        while (1) {
            int len = recv(client_sock, rx_buffer, RX_BUF_SIZE - 1, 0);

            if (len <= 0) {
                ESP_LOGI(TAG, "Client disconnected");
                break;
            }

            rx_buffer[len] = 0;
            ESP_LOGI(TAG, "RX: %s", rx_buffer);

            handle_command(rx_buffer, client_sock);
        }

        close(client_sock);
        g_tcp_client_sock = -1;
    }

    close(listen_sock);
    vTaskDelete(NULL);
}
