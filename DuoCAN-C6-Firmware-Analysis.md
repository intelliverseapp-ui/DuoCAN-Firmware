# DuoCAN-C6 Firmware Analysis

## Executive Summary

DuoCAN-C6 is an ESP-IDF 5.3.1 application targeting the single-core ESP32-C6. At startup it initializes two WS2812 status LEDs, creates a FreeRTOS queue and queue-consumer task, starts the TWAI CAN controller and CAN receive task, configures a Wi-Fi access point, and starts a TCP command server. TCP clients can issue basic control, status, and CAN transmit commands.

The most important functional issue is in the telemetry path: `tcp_queue_task()` removes a queue item and calls `tcp_server_send_line()`, which puts that item back into the same queue. As written, CAN RX telemetry is not sent to a TCP client; the queue consumer can circulate messages indefinitely. The accepted client socket is used for command responses, but no queue consumer sends data to it.

Other significant risks include a CAN receive loop limited by an unconditional 10 ms delay, enabled-but-unserviced TWAI alerts, a fixed Wi-Fi password, unreported command-operation failures, incomplete TCP send/error handling, and unsynchronized access to the shared WS2812 pixel buffer. The target configuration reports 48 RMT symbols per channel, while the WS2812 driver requests 64; verify that this is accepted by the configured IDF/target because channel creation is checked with `ESP_ERROR_CHECK` during startup.

This is a static review of the checked-in source and `sdkconfig`; no hardware execution, network test, load test, or source modification was performed.

## Architecture Review

The application entry point in [main/main.c](main/main.c) sequences subsystem initialization. The CAN and TCP implementations are in [main/duocan_can.c](main/duocan_can.c) and [main/tcp_server.c](main/tcp_server.c); [main/tcp_queue.c](main/tcp_queue.c) is intended to decouple CAN reception from socket transmission. LED control is split between [main/duocan_leds.c](main/duocan_leds.c) and [main/ws2812.c](main/ws2812.c).

The intended data/control flow appears to be:

- CAN RX: TWAI driver -> CAN receive task -> formatted text line -> outbound FreeRTOS queue -> TCP client.
- CAN TX/control: TCP command parser -> CAN API -> TWAI driver.
- Status: TCP command parser -> TWAI status API -> TCP command response.
- Indicators: CAN, TCP, and startup paths update shared WS2812 pixels.

The implemented RX flow stops at the queue: its consumer calls the queueing wrapper again instead of a socket-send operation. The declared `tcp_server_send_line()` comment and queue header describe a send path, but the implementation does not match that contract. The queue sender also runs before Wi-Fi or the TCP server starts, and is not tied to client connection state.

The component build registers six application sources. `led_test_harness.c` is present but is not included in `main/CMakeLists.txt`, so it is not part of the normal application build. `Kconfig.projbuild` defines USB/TinyUSB options, but the registered application does not use them. There is no checked-in project-level README or test suite in the inspected tree.

## FreeRTOS Task Model

The application creates three persistent tasks:

| Task | Stack request | Priority | Behavior |
|---|---:|---:|---|
| `can_rx_forward` | 4096 | 5 | Receives TWAI frames, formats telemetry, updates LEDs, logs, and queues a line. |
| `tcp_queue_task` | 4096 | 5 | Blocks on the outbound queue, then currently re-enqueues each item through the TCP wrapper. |
| `tcp_server` | 4096 | 10 | Accepts one client at a time and handles its command stream synchronously. |

The SDK configuration enables `FREERTOS_UNICORE` and sets the scheduler tick to 100 Hz (10 ms/tick). The TCP task's priority is notably higher than the CAN and queue tasks. Socket calls normally block and allow other tasks to run, but a high-priority task that repeatedly handles immediately available input and output can reduce CPU time available to CAN processing. Priorities should be justified with measured worst-case CAN load and socket behavior.

The CAN task calls `twai_receive()` with a 10 ms timeout and then delays another 10 ms on every iteration, including after successfully receiving a frame. This caps its ideal service rate near 100 frames/s before formatting, logging, LED updates, and scheduling overhead. That is low compared with the possible traffic rate of a 500 kbit/s CAN bus. The 16-entry TWAI RX queue can therefore fill during sustained or burst traffic.

The application declares 4096-byte stacks for each of its three worker tasks and leaves `app_main` running in a one-second delay loop. These requests exclude ESP-IDF Wi-Fi/lwIP/driver tasks, task control blocks, socket buffers, queue metadata, and RMT allocations. The configuration enables stack-overflow canary checks, but there is no runtime stack high-water or heap telemetry in the application.

There is no application-level task supervision or restart strategy. Failure to create a required task enters an infinite delay loop with an error LED. TCP socket setup failures instead delete only the TCP task, leaving the rest of the application running without a server.

## Queue, Buffer, and Memory Usage

The outbound queue has 64 entries of `tcp_queue_item_t`, each containing a 256-byte line array and a `size_t` length. On the 32-bit ESP32-C6 ABI this is approximately 260 bytes per item, or about 16.25 KiB of payload storage, plus FreeRTOS queue metadata and allocator overhead. The queue is dynamically allocated and has no explicit high-water monitoring or recovery policy. Pushes are non-blocking; full queues drop messages and only log a warning/update an LED.

The current consumer/requeue cycle prevents normal draining and can keep entries resident indefinitely. Producers can still fill remaining slots as they run; once full, subsequent CAN telemetry is dropped. The sender's attempt to requeue races with producers competing for the just-freed slot, so loss can occur even though the consumer is not delivering data.

Other notable buffers are the TCP server's 256-byte receive buffer and 512-byte command buffer, each on its task stack, plus a 128-byte formatted CAN line on the CAN task stack. A CAN frame line fits in the 256-byte queue item, but `tcp_queue_push()` silently truncates an overlong input to 255 bytes. Total declared application task stacks are 12 KiB, excluding `app_main`, Wi-Fi/lwIP/system tasks, socket buffers, queue metadata, and RMT allocations. The configuration enables stack-overflow canary checks, but there is no runtime stack high-water or heap telemetry in the application.

## TCP Server Lifecycle and Stability

The server creates an IPv4 stream socket on port 1234, enables `SO_REUSEADDR`, binds, listens with backlog 1, and accepts a single client. It processes that client's commands synchronously until `recv()` returns zero or an error, then closes the connection and returns to `accept()`. This is a simple single-client design; additional clients wait in the small listen backlog.

The global `g_tcp_client_sock` tracks the accepted socket but is not used by the queue consumer. Telemetry forwarding therefore does not depend on an active connection and is not transmitted. Socket setup errors terminate the task rather than retrying. `accept()` errors are logged and retried without delay, which could become a tight loop for persistent errors.

`safe_send()` accounts for partial writes, but treats only negative results as failure. A zero-byte send with remaining data makes no progress and can loop forever. Callers generally ignore its return value, so a failed command response is not reflected in connection handling. Socket calls have no explicit timeouts or application-level backpressure policy.

Command framing is newline-delimited and supports commands split across `recv()` calls. It does not strip a preceding carriage return, so clients sending CRLF can fail exact command matches. An overlong command causes an error and resets the parser, after which remaining bytes in that same receive may be interpreted as a new command. The single command-processing loop avoids concurrent command handlers but also means one slow client blocks acceptance of another.

## CAN TX/RX Pipeline

The TWAI driver is configured in normal mode at 500 kbit/s, with TX and RX queues of 16 and an accept-all filter. The transceiver enable GPIO is GPIO 6; TWAI TX and RX use GPIOs 4 and 5. Startup installs and starts the driver before launching the receive task.

RX forwards identifier, DLC, and payload bytes as decimal text in a `CAN_RX` line. It does not include frame format or other metadata such as standard/extended identifier or remote-frame status, which can make the text protocol insufficient to represent all accepted frames unambiguously. The 10 ms post-receive delay and per-frame info log increase latency and constrain throughput.

TX accepts standard identifiers only (`id <= 0x7FF`) and DLC values 0 through 8, then submits through `twai_transmit()` with a 20 ms wait. The API does not validate that `data` is non-NULL when DLC is nonzero. The TCP parser checks for at least an ID and DLC but does not require the stated number of data bytes; missing values remain zero, and byte values above 255 are truncated when cast. It also emits `SENT` even when the CAN API rejects or fails the transmit.

TWAI alerts for bus-off, RX queue full, error-passive, and RX data are enabled, but no code calls `twai_read_alerts()` or otherwise services them. Consequently the application does not act on bus-off or queue-full events, and does not provide alert-driven recovery or diagnostics. The exposed enable/disable functions invoke `twai_start()`/`twai_stop()` and toggle the transceiver, but their results are not propagated to the TCP client.

## Interrupt and Driver Usage

Peripheral work is performed through ESP-IDF driver APIs rather than application ISRs. TWAI is configured with `ESP_INTR_FLAG_LEVEL1`; there is no custom CAN interrupt handler. GPIO 6 is configured as an output for transceiver control. The WS2812 implementation uses the RMT TX driver at 20 MHz and waits synchronously for each transmission to complete.

The target config declares `SOC_RMT_MEM_WORDS_PER_CHANNEL=48`; [main/ws2812.c](main/ws2812.c) requests `mem_block_symbols=64`. This exceeds the target's reported per-channel symbol capacity and is a startup compatibility risk worth checking against the exact ESP-IDF 5.3.1 driver validation. `ESP_ERROR_CHECK(rmt_new_tx_channel(...))` makes a rejected configuration fatal during LED initialization. The driver also does not explicitly encode a WS2812 reset/latch interval; confirm the output-low timing between transmissions on the actual board.

Driver setup checks are inconsistent. Wi-Fi and RMT initialization use fatal ESP-IDF checks; TWAI setup returns errors to `app_main`, which then remains in an error loop. GPIO setup results are ignored. Queue creation/task creation failures are logged, but `tcp_queue_init()` returns `void`, so `app_main` continues even when telemetry infrastructure is unavailable.

## Concurrency and Race-Condition Risks

- The WS2812 mutex serializes `ws2812_show()`, but calls to `ws2812_set_pixel()` mutate the shared global `pixels` array before acquiring that mutex. CAN RX, TCP, and other task contexts can interleave pixel updates and transmission, producing lost or mixed LED states. The WS2812 legacy APIs that update and transmit directly do not use the LED mutex.
- The queue handle is initialized before the producer and queue task are started, which is good sequencing, but queue initialization failure is not made fatal to application startup. Producers then log on every attempted push.
- Only the TCP server task currently writes `g_tcp_client_sock`, and no separate task reads it for sends. It is therefore not presently a cross-task socket race; adding the intended queue sender will require a defined socket ownership/synchronization model, especially across disconnect and accept transitions.
- CAN operations are invoked by the RX task and TCP task concurrently through the TWAI driver. Driver calls are expected to provide their own synchronization, but command responses currently hide failures and there is no app-level serialization or lifecycle state around start/stop versus transmit.
- Several LED updates describe competing states: queue activity marks TCP up, queue overflow marks it down, and client connect/disconnect also sets those colors. The latest caller wins, so the indicator is not a reliable representation of a single authoritative state.

## Error Handling and Reconnection Behavior

Wi-Fi setup uses fatal ESP-IDF checks and does not register event handlers for station/client events, AP state changes, or recovery. The fixed SSID/password are logged at startup; the password `duocan123` is embedded in source and is weak for a device that may be deployed beyond a controlled lab.

The TCP listener does not restart after socket creation, bind, or listen failure. It does resume accepting after a client disconnect, but there is no explicit send-failure disconnect path. CAN bus-off is alerted but not recovered, and there is no retry/backoff policy for CAN startup or Wi-Fi failure. Commands for ENABLE_CAN and DISABLE_CAN always return success text irrespective of the driver result; SEND similarly reports success regardless of transmit outcome. STATUS is the exception in that it formats TWAI status errors.

## Maintainability Notes

The code is divided into small modules and has straightforward startup sequencing, but public contracts have drifted: the queue header describes a safe sender while the implementation requeues, and `tcp_server_send_line()` is named as a send operation though it only enqueues. CAN functions are manually redeclared in `tcp_server.c` despite an existing public header. Several comments and log messages describe intended behavior rather than the actual path, which makes review harder.

The TCP command protocol is ad hoc text with weak validation and no versioning or formal specification. Error reporting is inconsistent between logs, LEDs, and client responses. Hardware pins, AP credentials, queue sizing, task priorities, and CAN bit rate are compile-time literals rather than centrally documented board/runtime configuration. The unbuilt LED test harness duplicates `app_main`, which is acceptable only while it stays excluded from the registered component sources.

## Recommended Improvements

1. Correct the outbound architecture so one well-defined task owns socket writes and dequeues each item exactly once. Define behavior while disconnected (drop, bounded retention, or reconnect buffering), and remove the queue-to-queue call cycle.
2. Reduce CAN receive latency and remove the unconditional post-frame delay or make it conditional on timeout. Measure worst-case service rate, queue high-water marks, dropped frames, and CPU load under bus and Wi-Fi stress.
3. Add a TWAI alert-handling path. Report bus-off/error-passive/RX-overflow conditions and implement an explicit, bounded recovery policy, including transceiver and driver state coordination.
4. Harden command parsing: strip CRLF, validate token count and numeric ranges, require the declared data bytes, reject malformed/extra fields, and return actual driver results. Validate non-NULL TX payloads for nonzero DLC.
5. Make socket operations bounded and progress-safe: handle zero-byte sends, check every send result, close/reaccept on fatal connection errors, and add backoff for repeated accept/setup failures.
6. Establish a single owner or lock-protected API for WS2812 pixel-buffer mutation and transmission. Make status indicators derive from explicit subsystem state rather than unrelated callers overwriting colors.
7. Verify RMT symbol sizing against the ESP32-C6/IDF 5.3.1 driver and define/test the required WS2812 reset interval on the hardware.
8. Treat queue and task initialization failure consistently; return status from subsystem initialization and avoid reporting the system ready when required services are absent.
9. Move deployment-sensitive Wi-Fi credentials and board/rate settings into an appropriate configuration mechanism; document provisioning and security expectations.
10. Add focused host/parser tests and hardware integration tests for queue delivery, disconnect/reconnect, CAN bus-off recovery, malformed commands, full queues, RMT startup, and sustained CAN traffic. Record stack high-water and heap minimums during stress testing.

## Review Limitations

This review did not build or flash the firmware and did not exercise CAN, Wi-Fi, TCP, or RMT hardware. Throughput and memory figures above are source/configuration-derived estimates, not measured runtime results. The generated `build/` directory was not treated as source-of-truth evidence.