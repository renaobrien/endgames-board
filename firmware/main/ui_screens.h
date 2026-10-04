/* SPDX-License-Identifier: MIT */
#pragma once
#include "lvgl.h"

/* Wi-Fi setup: SSID field, password field, on-screen keyboard, Connect button.
 * on_connect is called with the typed credentials; the caller stores them in NVS. */
typedef void (*eg_wifi_cb_t)(const char *ssid, const char *password);
lv_obj_t *eg_wifi_create(lv_obj_t *parent, eg_wifi_cb_t on_connect);
void eg_wifi_set_error(lv_obj_t *screen, const char *msg);   /* NULL clears */

/* Pairing: big 6-char code plus the URL to type it on. */
lv_obj_t *eg_pair_create(lv_obj_t *parent);
void eg_pair_set_code(lv_obj_t *screen, const char *code, const char *claim_url);
void eg_pair_set_status(lv_obj_t *screen, const char *msg);

/* Idle: "No game in progress" with the owner's set on show. */
lv_obj_t *eg_idle_create(lv_obj_t *parent);
void eg_idle_refresh(lv_obj_t *screen);   /* re-reads pieces after the set changes */
