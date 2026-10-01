#pragma once
#include <stddef.h>

// TCP server task (main listener)
void tcp_server_task(void *arg);

// Safe CAN RX forwarding function (used by duocan_can.c)
void tcp_server_send_line(const char *line);
