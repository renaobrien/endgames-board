/* SPDX-License-Identifier: MIT */
#pragma once
#include "lvgl.h"

/* Wi-Fi setup: list of nearby networks (tap to pick), name and password fields, on-screen keyboard,
 * Connect and Scan again. on_connect gets the credentials (the caller stores them in NVS);
 * on_rescan asks the caller to scan again and call eg_wifi_set_networks. */
#include "bsp.h"
typedef void (*eg_wifi_cb_t)(const char *ssid, const char *password);
lv_obj_t *eg_wifi_create(lv_obj_t *parent, eg_wifi_cb_t on_connect, void (*on_rescan)(void), void (*on_back)(void));
void eg_wifi_set_back_visible(lv_obj_t *screen, bool visible);   /* Back shows only when there's a working network to go back to */

/* Pairing and idle screens get a "Change Wi-Fi" button that calls this. */
void eg_screens_set_wifi_handler(void (*on_wifi)(void));
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

/* ---------- home and its sub-screens ---------- */
#include "api.h"

typedef struct {
    void (*on_play_ai)(void);                 /* open the computer setup screen */
    void (*on_challenge)(void);               /* open the challenge screen and create a link */
    void (*on_open_game)(const char *game_id);
} eg_home_cb_t;

lv_obj_t *eg_home_create(lv_obj_t *parent, const eg_home_cb_t *cb);
void eg_home_set(lv_obj_t *screen, const eg_home_t *h);
void eg_home_set_status(lv_obj_t *screen, const char *msg);  /* NULL clears */

/* Play the computer: difficulty + color, then Start. */
lv_obj_t *eg_ai_setup_create(lv_obj_t *parent, void (*on_start)(const char *difficulty, const char *color), void (*on_back)(void));
void eg_ai_setup_set_status(lv_obj_t *screen, const char *msg);

/* Challenge a friend: QR of the challenge link. */
lv_obj_t *eg_challenge_create(lv_obj_t *parent, void (*on_back)(void));
void eg_challenge_set(lv_obj_t *screen, const char *url, const char *status);   /* url NULL hides the QR */

/* Tabs (same as the website): 0 Play, 1 Make, 2 Sets, 3 Rank, 4 You. */
void eg_screens_set_tab_handler(void (*on_tab)(int tab));

lv_obj_t *eg_sets_create(lv_obj_t *parent, void (*on_pick)(const char *set_id));
void eg_sets_set(lv_obj_t *screen, const eg_home_t *h);
lv_obj_t *eg_make_create(lv_obj_t *parent);
lv_obj_t *eg_rank_create(lv_obj_t *parent);
void eg_rank_set(lv_obj_t *screen, const eg_rank_t *d, const char *error);   /* error non-NULL shows it instead */
lv_obj_t *eg_you_create(lv_obj_t *parent, void (*on_forget)(void), const char *version);
void eg_you_set(lv_obj_t *screen, const eg_home_t *h);

const char *eg_difficulty_label(const char *api_value);   /* beginner -> Easy, ... */
