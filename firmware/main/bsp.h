/* SPDX-License-Identifier: MIT */
#pragma once
#include "lvgl.h"

/* Display + touch bring-up for the Elecrow CrowPanel Advanced 5" ESP32-P4 (800x480 RGB, GT911 touch)
 * and the LVGL port. Returns the display, or NULL on failure. */
lv_display_t *eg_bsp_init(void);

/* Wi-Fi comes up through the onboard ESP32-C6 (ESP-Hosted). Blocks until connected or timeout_ms. */
bool eg_bsp_wifi_connect(const char *ssid, const char *pass, int timeout_ms);
