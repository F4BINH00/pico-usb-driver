
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

// TinyUSB configuration for RP2040 device (Pico SDK)

#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE)
#define CFG_TUSB_OS                OPT_OS_PICO

#define CFG_TUD_ENDPOINT0_SIZE     64

// Device class
#define CFG_TUD_VENDOR             1
#define CFG_TUD_CDC                0
#define CFG_TUD_MSC                0
#define CFG_TUD_HID                0

// Vendor endpoints
#define CFG_TUD_VENDOR_RX_BUFSIZE  512
#define CFG_TUD_VENDOR_TX_BUFSIZE  512

#ifdef __cplusplus
 }
#endif

#endif
