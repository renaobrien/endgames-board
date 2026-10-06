/* SPDX-License-Identifier: MIT */
#pragma once
#include "lvgl.h"

/* Slim header on every screen: optional Back on the left, title in the accent color. */
#define EG_HDR_H 50
lv_obj_t *eg_header(lv_obj_t *screen, const char *title, lv_color_t accent, lv_event_cb_t back_cb, void *ud, lv_obj_t **back_out);
void eg_header_back_text(lv_obj_t *back, const char *text);

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
    void (*on_challenge)(void);               /* open the challenge screen and create a link (QR) */
    void (*on_open_game)(const char *game_id);
    void (*on_quick_match)(void);             /* open the quick match screen and join the queue */
    void (*on_find_player)(void);             /* open the player search screen */
    void (*on_respond)(const char *challenge_id, bool accept);   /* incoming challenge: Accept / Decline */
} eg_home_cb_t;

lv_obj_t *eg_home_create(lv_obj_t *parent, const eg_home_cb_t *cb);
void eg_home_set(lv_obj_t *screen, const eg_home_t *h);
void eg_home_set_status(lv_obj_t *screen, const char *msg);  /* NULL clears */

/* Play the computer: difficulty + color, then Start. */
lv_obj_t *eg_ai_setup_create(lv_obj_t *parent, void (*on_start)(const char *difficulty, const char *color), void (*on_back)(void));
void eg_ai_setup_set_status(lv_obj_t *screen, const char *msg);

/* Send a link: QR of the challenge link. Opened from Challenge a player; Back returns there. */
lv_obj_t *eg_challenge_create(lv_obj_t *parent, void (*on_back)(void));
void eg_challenge_set(lv_obj_t *screen, const char *url, const char *status);   /* url NULL hides the QR */

/* Quick match: pick a clock, then "Looking for an opponent..." with Cancel.
 * on_join gets the time control ("" untimed). searching false shows Try again and turns Cancel into Back. */
lv_obj_t *eg_qm_create(lv_obj_t *parent, void (*on_join)(const char *time_control), void (*on_cancel)(void), void (*on_retry)(void));
void eg_qm_pick(lv_obj_t *screen);       /* back to the clock picker */
void eg_qm_set(lv_obj_t *screen, const char *msg, bool searching);

/* Challenge a player: search by name, pick a player, pick who goes first, send. */
typedef struct {
    void (*on_query)(const char *q);                                       /* every edit; the caller debounces */
    void (*on_send)(const char *user_id, const char *name, const char *first, const char *time_control);   /* first: me | computer | random; time_control "" = untimed */
    void (*on_link)(void);                                                 /* "Share a link" (the QR challenge) */
    void (*on_back)(void);
} eg_find_cb_t;
lv_obj_t *eg_find_create(lv_obj_t *parent, const eg_find_cb_t *cb);
void eg_find_reset(lv_obj_t *screen);                                       /* empty search, keyboard up */
void eg_find_set_results(lv_obj_t *screen, const eg_user_t *users, int n, const char *msg);   /* msg non-NULL shows instead of an empty list */
void eg_find_set_status(lv_obj_t *screen, const char *msg, bool sent);   /* sent: hides Send, Back goes home */
void eg_find_confirm(lv_obj_t *screen, int index);                         /* open the confirm panel for result index (preview) */

/* Tabs (same as the website): 0 Play, 1 Sets, 2 Rank, 3 You. */
void eg_screens_set_tab_handler(void (*on_tab)(int tab));
void eg_screens_set_badge(bool on);      /* pink dot on Play: a challenge or your move is waiting */

lv_obj_t *eg_sets_create(lv_obj_t *parent, void (*on_pick)(const char *set_id), void (*on_make)(void));
void eg_sets_set(lv_obj_t *screen, const eg_home_t *h);
/* Rename / delete one of your sets (the Edit pill on each tile). Call eg_sets_edit_done with the result. */
void eg_sets_set_edit(lv_obj_t *screen, void (*on_rename)(const char *id, const char *name), void (*on_delete)(const char *id));
void eg_sets_edit_done(lv_obj_t *screen, bool ok, const char *msg);
lv_obj_t *eg_make_create(lv_obj_t *parent, void (*on_back)(void));
lv_obj_t *eg_rank_create(lv_obj_t *parent);
void eg_rank_set(lv_obj_t *screen, const eg_rank_t *d, const char *error);   /* error non-NULL shows it instead */
lv_obj_t *eg_you_create(lv_obj_t *parent, void (*on_forget)(void), const char *version);
void eg_you_set(lv_obj_t *screen, const eg_home_t *h);
void eg_test_pattern(void);   /* 1 px edges: checks the picture is centered (5 taps on the firmware line in You) */

const char *eg_difficulty_label(const char *api_value);   /* beginner -> Easy, ... */

/* Boot screen: shown the moment the display is up. Pixel queen spinning, wordmark, one status line. */
lv_obj_t *eg_boot_create(lv_obj_t *parent);
void eg_boot_set_status(lv_obj_t *screen, const char *msg);
