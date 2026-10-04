/* SPDX-License-Identifier: MIT */
/* Board support. This is the one file that depends on the exact hardware.
 *
 * NOT IMPLEMENTED YET: needs the real board. Start from Elecrow's sample for this board
 * (github.com/Elecrow-RD/-CrowPanel-Advanced-5inch-ESP32-P4-HMI-AI-Display-800x480-IPS-Touch-Screen):
 *   1. eg_bsp_init: copy the MIPI/RGB panel init, backlight and GT911 touch init from the sample,
 *      then register them with esp_lvgl_port (lvgl_port_add_disp / lvgl_port_add_touch).
 *   2. eg_bsp_wifi_connect: esp_hosted + esp_wifi_remote station connect (the sample has the SDIO pins).
 * Check the sample's license before copying code, and keep this repo MIT-compatible.
 * Everything above this file (UI, API, storage) is hardware independent and already written.
 */
#include "bsp.h"
#include "esp_log.h"

lv_display_t *eg_bsp_init(void)
{
    ESP_LOGE("eg_bsp", "eg_bsp_init is not implemented. See the comment at the top of bsp.c.");
    return NULL;
}

bool eg_bsp_wifi_connect(const char *ssid, const char *pass, int timeout_ms)
{
    (void)ssid; (void)pass; (void)timeout_ms;
    ESP_LOGE("eg_bsp", "eg_bsp_wifi_connect is not implemented. See the comment at the top of bsp.c.");
    return false;
}
