#pragma once
#include <stddef.h>
#include <sys/types.h>

// ------------------------------------------------------------
// GLOBALS EXPOSED TO OTHER MODULES
// ------------------------------------------------------------

// The active TCP client socket (or -1 if none)
extern int g_tcp_client_sock;

// Safe send helper used by queue consumer
int safe_send(int sock, const char *data, size_t len);

// ------------------------------------------------------------
// TCP SERVER TASK (main listener)
// ------------------------------------------------------------
void tcp_server_task(void *arg);

// ------------------------------------------------------------
// CAN RX forwarding function (used by duocan_can.c)
// ------------------------------------------------------------
void tcp_server_send_line(const char *line);
