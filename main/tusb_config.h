#pragma once

// ---------------------------------------------------------
// TinyUSB configuration for DuoCAN ESP32-C6
// ---------------------------------------------------------
//
// This file defines the TinyUSB CDC ACM configuration used
// by DuoCAN so that Android (BabyNodeAutomotive) can detect
// the device as a USB serial port.
//
// ESP-IDF + TinyUSB requires this file to exist in your
// component include path (main/).
//
// ---------------------------------------------------------

// Enable TinyUSB device stack
#define CFG_TUD_ENABLED 1

// Enable CDC ACM class driver
#define CFG_TUD_CDC 1

// USB buffer sizes (safe defaults)
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256

// Number of CDC interfaces (1 = DuoCAN)
#define CFG_TUD_CDC_EP_BUFSIZE 64

// TinyUSB internal options
#define CFG_TUSB_OS_NONE 1
#define CFG_TUSB_MCU_ESP32C6 1

// No RTOS inside TinyUSB (we use FreeRTOS externally)
#define CFG_TUSB_DEBUG 0
