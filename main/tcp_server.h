#pragma once

// TCP server task
void tcp_server_task(void *arg);

// CAN RX forwarding function (used by duocan_can.c)
void tcp_server_send_line(const char *line);
