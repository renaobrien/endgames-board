/* SPDX-License-Identifier: MIT */
#pragma once
#include "lvgl.h"

/* Wi-Fi setup: list of nearby networks (tap to pick), name and password fields, on-screen keyboard,
 * Connect and Scan again. on_connect gets the credentials (the caller stores them in NVS);
 * on_rescan asks the caller to scan again and call eg_wifi_set_networks. */
#include "bsp.h"
typedef void (*eg_wifi_cb_t)(const char *ssid, const char *password);
lv_obj_t *eg_wifi_create(lv_obj_t *parent, eg_wifi_cb_t on_connect, void (*on_rescan)(void));
void eg_wifi_set_error(lv_obj_t *screen, const char *msg);   /* NULL clears */
void eg_wifi_set_scanning(lv_obj_t *screen);
void eg_wifi_set_networks(lv_obj_t *screen, const eg_ap_t *aps, int n);

/* Pairing: big 6-char code plus the URL to type it on. */
lv_obj_t *eg_pair_create(lv_obj_t *parent);
void eg_pair_set_code(lv_obj_t *screen, const char *code, const char *claim_url);
void eg_pair_set_status(lv_obj_t *screen, const char *msg);

/* Idle: "No game in progress" with the owner's set on show. */
lv_obj_t *eg_idle_create(lv_obj_t *parent);
void eg_idle_refresh(lv_obj_t *screen);   /* re-reads pieces after the set changes */
