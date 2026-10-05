/* SPDX-License-Identifier: MIT */
/* Endgames Board firmware: boot, Wi-Fi, pairing, then play.
 * One network task owns all HTTP. The UI runs in LVGL's task; every LVGL call from the network
 * task is wrapped in lvgl_port_lock / unlock. */
#include "api.h"
#include "bsp.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "ota.h"
#include "fonts.h"
#include "theme.h"
#include "pieces_store.h"
#include "store.h"
#include "ui_game.h"
#include "ui_screens.h"
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

static const char *TAG = "eg";

static char token[96];
static lv_obj_t *scr_wifi, *scr_pair, *scr_home, *scr_ai, *scr_chal, *scr_qm, *scr_find, *scr_sets, *scr_make, *scr_rank, *scr_you, *scr_game;
static eg_rank_t rank;
static eg_home_t home;                  /* last board-home result */
static eg_game_t cur;
static bool have_game;
static volatile bool want_wifi_screen, want_wifi_back;


static void show(lv_obj_t *s)
{
    lvgl_port_lock(0);
    lv_screen_load(s);
    lvgl_port_unlock();
}

/* Main screens (pairing, idle, game). If the Wi-Fi screen is open on top, remember it for Back instead. */
static lv_obj_t *behind;
static bool wifi_open;
static void show_main(lv_obj_t *s)
{
    behind = s;
    if (!wifi_open) show(s);
}

/* ---------- UI callbacks (run in LVGL task) ---------- */

/* Connect runs in the network task (it blocks up to 20 s); the UI only hands over the credentials. */
static char want_ssid[33], want_pass[65];
static volatile bool creds_pending;

static void on_wifi_connect(const char *ssid, const char *pass)
{
    strncpy(want_ssid, ssid, sizeof want_ssid - 1);
    want_ssid[sizeof want_ssid - 1] = 0;
    strncpy(want_pass, pass, sizeof want_pass - 1);
    want_pass[sizeof want_pass - 1] = 0;
    creds_pending = true;
}

static void on_move(const char *from, const char *to, char promo)
{
    /* Hand off to the network task via a small queue-free flag set: moves are rare, so use a one-slot mailbox. */
    extern void eg_net_submit_move(const char *, const char *, char);
    eg_net_submit_move(from, to, promo);
}

static void on_forget(void)
{
    eg_store_clear_token();
    esp_restart();
}

/* ---------- Wi-Fi scan (own task: a scan blocks for a few seconds) ---------- */

static TaskHandle_t scan_task_h;

static void scan_task(void *arg)
{
    (void)arg;
    static eg_ap_t aps[20];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        lvgl_port_lock(0);
        eg_wifi_set_scanning(scr_wifi);
        lvgl_port_unlock();
        int n = eg_bsp_wifi_scan(aps, 20);
        lvgl_port_lock(0);
        eg_wifi_set_networks(scr_wifi, aps, n);
        lvgl_port_unlock();
    }
}

static void request_scan(void)
{
    if (scan_task_h) xTaskNotifyGive(scan_task_h);
}

static void on_wifi_back(void)
{
    want_wifi_back = true;
}

static void on_menu_wifi(void)
{
    want_wifi_screen = true;
}

/* ---------- home navigation (UI task) ----------
 * Sub-screens open instantly from the UI; anything that needs the server becomes a request
 * the network task picks up within 250 ms. */

static struct {
    volatile bool home, ai, challenge, open, set, resign, rank, thumbs;
    volatile bool qm_join, qm_cancel, search, chal_send, respond, respond_accept;
    char difficulty[16], color[8], game_id[40], set_id[48];
    char query[32], opp_id[40], opp_name[33], first[10], chal_id[40];
    TickType_t search_at;               /* debounce: search when the typing stops for 400 ms */
} req;

static void ui_load(lv_obj_t *s) { lv_screen_load(s); }   /* already in the LVGL task */

static void on_home_play_ai(void) { eg_ai_setup_set_status(scr_ai, NULL); ui_load(scr_ai); }
static void on_home_challenge(void)
{
    eg_challenge_set(scr_chal, NULL, "Making a link...");
    ui_load(scr_chal);
    snprintf(req.color, sizeof req.color, "random");
    req.challenge = true;
}
/* Quick match. Cancel drops a pending join (rapid Quick match, Cancel taps) and goes home at once;
 * the network task cancels the queue entry, and opens the game if a match landed in between. */
static void on_home_quick(void)
{
    eg_qm_set(scr_qm, "Looking for an opponent...", true);
    ui_load(scr_qm);
    req.qm_join = true;
}
static void on_qm_cancel(void)
{
    req.qm_join = false;
    req.qm_cancel = true;
    behind = scr_home;
    ui_load(scr_home);
}
static void on_qm_retry(void) { on_home_quick(); }

/* Challenge a player */
static void on_home_find(void) { eg_find_reset(scr_find); ui_load(scr_find); }
static void on_find_query(const char *q)
{
    snprintf(req.query, sizeof req.query, "%s", q);
    req.search_at = xTaskGetTickCount() + pdMS_TO_TICKS(400);
    req.search = strlen(req.query) >= 2;
}
static void on_find_send(const char *id, const char *name, const char *first)
{
    snprintf(req.opp_id, sizeof req.opp_id, "%s", id);
    snprintf(req.opp_name, sizeof req.opp_name, "%s", name[0] ? name : "them");
    snprintf(req.first, sizeof req.first, "%s", first);
    req.chal_send = true;
}
static void on_respond(const char *id, bool accept)
{
    snprintf(req.chal_id, sizeof req.chal_id, "%s", id);
    req.respond_accept = accept;
    req.respond = true;
}

static void on_tab(int tab)
{
    lv_obj_t *screens[5] = {scr_home, scr_make, scr_sets, scr_rank, scr_you};
    if (tab < 0 || tab > 4) return;
    if (tab == 2) { eg_sets_set(scr_sets, &home); req.thumbs = true; }
    if (tab == 3) req.rank = true;
    if (tab == 4) eg_you_set(scr_you, &home);
    if (tab == 0) req.home = true;
    behind = screens[tab];                 /* Wi-Fi's Back returns here */
    ui_load(screens[tab]);
}
static void on_home_open(const char *id) { snprintf(req.game_id, sizeof req.game_id, "%s", id); req.open = true; }
static void on_sub_back(void) { req.search = false; req.home = true; behind = scr_home; ui_load(scr_home); }
static void on_ai_start(const char *difficulty, const char *color)
{
    snprintf(req.difficulty, sizeof req.difficulty, "%s", difficulty);
    snprintf(req.color, sizeof req.color, "%s", color);
    req.ai = true;
}
static void on_pick_set(const char *id) { snprintf(req.set_id, sizeof req.set_id, "%s", id); req.set = true; }
static void on_menu_home(void) { req.home = true; ui_load(scr_home); }
static void on_resign(void) { req.resign = true; }

/* ---------- network task ---------- */

static struct { bool pending; char from[3], to[3], promo; } mailbox;

void eg_net_submit_move(const char *from, const char *to, char promo)
{
    memcpy(mailbox.from, from, 3);
    memcpy(mailbox.to, to, 3);
    mailbox.promo = promo;
    mailbox.pending = true;
}

static void apply_game(const eg_game_t *g)
{
    cur = *g;
    have_game = true;
    lvgl_port_lock(0);
    eg_game_set(scr_game, g);
    behind = scr_game;
    if (!wifi_open) lv_screen_load(scr_game);
    lvgl_port_unlock();
}

static void handle_wifi(void);

static bool pair(void)
{
    eg_pairing_t p;
    show_main(scr_pair);
    while (eg_api_pair_start(&p) != EG_OK) {
        for (int i = 0; i < 20; i++) { vTaskDelay(pdMS_TO_TICKS(250)); handle_wifi(); }
    }
    lvgl_port_lock(0);
    eg_pair_set_code(scr_pair, p.code, p.claim_url[0] ? p.claim_url : NULL);
    lvgl_port_unlock();
    for (;;) {
        for (int i = 0; i < 12; i++) { vTaskDelay(pdMS_TO_TICKS(250)); handle_wifi(); }
        eg_result_t r = eg_api_pair_poll(p.poll_secret, token);
        if (r == EG_OK) { eg_store_save_token(token); return true; }
        if (r == EG_EXPIRED) return false;      /* caller starts over with a fresh code */
    }
}

static void wifi_error(const char *msg)
{
    lvgl_port_lock(0);
    eg_wifi_set_error(scr_wifi, msg);
    lvgl_port_unlock();
}

/* Try the typed credentials. Saves them only if they work. */
static bool try_pending_creds(void)
{
    creds_pending = false;
    if (eg_bsp_wifi_connect(want_ssid, want_pass, 20000)) {
        eg_store_save_wifi(want_ssid, want_pass);
        wifi_error(NULL);
        return true;
    }
    wifi_error("Couldn't connect. Check the password.");
    if (wifi_open) {                      /* switching from a working network: get back on it meanwhile */
        char ssid[33] = {0}, pass[65] = {0};
        if (eg_store_load_wifi(ssid, pass) && ssid[0]) eg_bsp_wifi_connect(ssid, pass, 20000);
    }
    return false;
}

/* Wi-Fi screen opened from a main screen: Back, or a new network that works, returns there. */
static void handle_wifi(void)
{
    if (want_wifi_screen) {
        want_wifi_screen = false;
        wifi_open = true;
        lvgl_port_lock(0);
        eg_wifi_set_back_visible(scr_wifi, true);
        eg_wifi_set_error(scr_wifi, NULL);
        lv_screen_load(scr_wifi);
        lvgl_port_unlock();
        request_scan();
    }
    if (want_wifi_back) {
        want_wifi_back = false;
        if (wifi_open) { wifi_open = false; if (behind) show(behind); }
    }
    if (creds_pending && wifi_open) {
        if (try_pending_creds()) { wifi_open = false; if (behind) show(behind); }
    }
}

/* Saved network first; otherwise the Wi-Fi screen until a network works. */
static void ensure_wifi(void)
{
    char ssid[33] = {0}, pass[65] = {0};
#ifdef EG_DEV_WIFI_SSID
    if (!eg_store_load_wifi(ssid, pass) && EG_DEV_WIFI_SSID[0]) { strcpy(ssid, EG_DEV_WIFI_SSID); strcpy(pass, EG_DEV_WIFI_PASS); }
#else
    eg_store_load_wifi(ssid, pass);
#endif
    if (ssid[0] && eg_bsp_wifi_connect(ssid, pass, 20000)) return;
    show(scr_wifi);
    request_scan();
    if (ssid[0]) wifi_error("Couldn't reach your saved network. Pick one.");
    for (;;) {
        while (!creds_pending) vTaskDelay(pdMS_TO_TICKS(100));
        if (try_pending_creds()) return;
    }
}

/* ---------- updates ---------- */

static void show_updating(const char *version)
{
    lvgl_port_lock(0);
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, EG_BG_STAGE, 0);
    lv_obj_t *t = lv_label_create(s);
    lv_obj_set_style_text_font(t, &eg_bungee_28, 0);
    lv_obj_set_style_text_color(t, EG_YELLOW, 0);
    lv_label_set_text(t, "UPDATING");
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -24);
    lv_obj_t *v = lv_label_create(s);
    lv_obj_set_style_text_font(v, &eg_sora_20, 0);
    lv_obj_set_style_text_color(v, EG_FG_HAZE, 0);
    lv_label_set_text_fmt(v, "Version %s. Keep it plugged in.", version);
    lv_obj_align(v, LV_ALIGN_CENTER, 0, 24);
    lv_screen_load(s);
    lvgl_port_unlock();
}

#define UPDATE_CHECK_MS (6 * 60 * 60 * 1000)

static void net_task(void *arg)
{
    (void)arg;
    ensure_wifi();
    eg_ota_mark_good();                         /* this version gets online: keep it */
    eg_ota_check_and_update(show_updating);     /* restarts if there is a newer one */
    TickType_t last_update_check = xTaskGetTickCount();
    eg_store_load_token(token);
    for (;;) {
        if (!token[0]) {
            while (!pair()) { /* expired: loop with a new code */ }
        }
        bool changed;
        eg_pieces_load(token, &changed);

        enum { V_HOME, V_GAME, V_CHAL, V_QM } view = V_HOME;
        TickType_t qm_started = 0, qm_next = 0;
        bool unauthorized = false;
        TickType_t next_poll = 0;
        static eg_game_t scratch;                /* big struct: keep it off the task stack */
        static char chal_seen[EG_HOME_MAX_GAMES][40];
        int chal_seen_n = -1;                    /* games that existed when the challenge link was made */
        memset((void *)&req, 0, sizeof req);
        req.home = true;

        while (!unauthorized) {
            handle_wifi();
            TickType_t now = xTaskGetTickCount();
            eg_result_t r = EG_OK;

            if (req.home) {                      /* back to home: refresh the list */
                req.home = false;
                view = V_HOME;
                have_game = false;
                if (behind != scr_home) show_main(scr_home);
                next_poll = 0;
            }
            if (req.open) {
                req.open = false;
                r = eg_api_game_id(token, req.game_id, &scratch);
                if (r == EG_OK) { apply_game(&scratch); view = V_GAME; next_poll = now + pdMS_TO_TICKS(5000); }
                else if (r != EG_UNAUTHORIZED) { lvgl_port_lock(0); eg_home_set_status(scr_home, "Couldn't open that game."); lvgl_port_unlock(); }
            }
            if (req.ai) {
                req.ai = false;
                r = eg_api_new_ai(token, req.difficulty, req.color, &scratch);
                if (r == EG_OK) { apply_game(&scratch); view = V_GAME; next_poll = now + pdMS_TO_TICKS(3000); }
                else if (r != EG_UNAUTHORIZED) { lvgl_port_lock(0); eg_ai_setup_set_status(scr_ai, "Couldn't start the game. Try again."); lvgl_port_unlock(); }
            }
            if (req.challenge) {
                req.challenge = false;
                char url[160];
                r = eg_api_new_challenge(token, req.color, url);
                lvgl_port_lock(0);
                if (r == EG_OK) eg_challenge_set(scr_chal, url, "Waiting for them to join...");
                else if (r != EG_UNAUTHORIZED) eg_challenge_set(scr_chal, NULL, "Couldn't make a link. Go back and try again.");
                lvgl_port_unlock();
                if (r == EG_OK) {
                    view = V_CHAL;
                    chal_seen_n = home.n_games;
                    for (int i = 0; i < home.n_games; i++) snprintf(chal_seen[i], sizeof chal_seen[i], "%s", home.games[i].id);
                    next_poll = now + pdMS_TO_TICKS(8000);
                }
            }
            if (req.qm_cancel) {
                req.qm_cancel = false;
                if (view == V_QM) { view = V_HOME; next_poll = 0; }
                if (view != V_GAME) {                 /* already in a matched game: the queue entry is gone */
                    eg_qm_status_t qs;
                    r = eg_api_quick_match(token, "cancel", &qs, &scratch);
                    if (r == EG_OK) {
                        /* cancel only removes an unmatched entry; a match that landed first still opens */
                        r = eg_api_quick_match(token, NULL, &qs, &scratch);
                        if (r == EG_OK && qs == EG_QM_MATCHED) {
                            req.qm_join = false;
                            apply_game(&scratch); view = V_GAME; next_poll = now + pdMS_TO_TICKS(5000);
                        }
                    }
                }
                if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }
            }
            if (req.qm_join) {
                req.qm_join = false;
                eg_qm_status_t qs;
                r = eg_api_quick_match(token, "join", &qs, &scratch);
                if (r == EG_OK && qs == EG_QM_MATCHED) {
                    apply_game(&scratch); view = V_GAME; next_poll = now + pdMS_TO_TICKS(5000);
                } else if (r == EG_OK && qs == EG_QM_WAITING) {
                    view = V_QM; qm_started = now; qm_next = now + pdMS_TO_TICKS(3000);
                    next_poll = now + pdMS_TO_TICKS(30000);
                } else if (r == EG_NO_GAME) {
                    lvgl_port_lock(0);
                    eg_qm_set(scr_qm, "Matched! Open the game from Your games.", false);
                    lvgl_port_unlock();
                    next_poll = 0;
                } else if (r != EG_UNAUTHORIZED) {
                    lvgl_port_lock(0);
                    eg_qm_set(scr_qm, "Couldn't join quick match. Check the connection and try again.", false);
                    lvgl_port_unlock();
                }
                if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }
            }
            if (view == V_QM && (int32_t)(now - qm_next) >= 0) {
                eg_qm_status_t qs;
                r = eg_api_quick_match(token, NULL, &qs, &scratch);
                uint32_t secs = (now - qm_started) / configTICK_RATE_HZ;
                if (r == EG_OK && qs == EG_QM_MATCHED) {
                    apply_game(&scratch); view = V_GAME; next_poll = now + pdMS_TO_TICKS(5000);
                } else if (r == EG_NO_GAME) {
                    view = V_HOME; next_poll = 0;
                    lvgl_port_lock(0); eg_qm_set(scr_qm, "Matched! Open the game from Your games.", false); lvgl_port_unlock();
                } else if ((r == EG_OK && qs == EG_QM_IDLE) || secs >= 5 * 60) {
                    /* the server drops entries after 5 minutes; stop on our side too */
                    if (secs >= 5 * 60) eg_api_quick_match(token, "cancel", &qs, &scratch);
                    view = V_HOME;
                    lvgl_port_lock(0); eg_qm_set(scr_qm, "No one else is looking right now. Try again, or play the computer.", false); lvgl_port_unlock();
                } else if (r != EG_UNAUTHORIZED) {
                    char msg[96];
                    if (r == EG_OK) snprintf(msg, sizeof msg, "Looking for an opponent... %lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
                    else snprintf(msg, sizeof msg, "Lost the connection. Still trying... %lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
                    lvgl_port_lock(0); eg_qm_set(scr_qm, msg, true); lvgl_port_unlock();
                    qm_next = now + pdMS_TO_TICKS(3000);
                }
                if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }
            }
            if (req.search && (int32_t)(now - req.search_at) >= 0) {
                req.search = false;
                char q[32];
                lvgl_port_lock(0);
                snprintf(q, sizeof q, "%s", req.query);
                lvgl_port_unlock();
                static eg_user_t users[EG_USERS_MAX];
                int n = 0;
                r = eg_api_users(token, q, users, &n);
                lvgl_port_lock(0);
                if (strcmp(q, req.query) == 0 && r != EG_UNAUTHORIZED)   /* drop answers to text that has changed since */
                    eg_find_set_results(scr_find, users, n, r == EG_OK ? "No players with that name." : "Couldn't search. Check the connection.");
                lvgl_port_unlock();
                if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }
            }
            if (req.chal_send) {
                req.chal_send = false;
                r = eg_api_challenge_player(token, req.opp_id, req.first);
                char msg[160];
                lvgl_port_lock(0);
                if (r == EG_OK) {
                    snprintf(msg, sizeof msg, "Sent. Waiting for %s to accept. The game opens here when they do, and it shows in Your games.", req.opp_name);
                    eg_find_set_status(scr_find, msg, true);
                } else if (r == EG_CONFLICT) {
                    snprintf(msg, sizeof msg, "You already challenged %s. Waiting for them to answer.", req.opp_name);
                    eg_find_set_status(scr_find, msg, true);
                } else if (r != EG_UNAUTHORIZED) {
                    eg_find_set_status(scr_find, "Couldn't send the challenge. Try again.", false);
                }
                lvgl_port_unlock();
                if (r == EG_OK || r == EG_CONFLICT) {     /* watch for the game, like the QR challenge */
                    view = V_CHAL;
                    chal_seen_n = home.n_games;
                    for (int i = 0; i < home.n_games; i++) snprintf(chal_seen[i], sizeof chal_seen[i], "%s", home.games[i].id);
                    next_poll = now + pdMS_TO_TICKS(8000);
                }
                if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }
            }
            if (req.respond) {
                req.respond = false;
                bool accept = req.respond_accept;
                r = eg_api_challenge_respond(token, req.chal_id, accept, &scratch);
                if (r == EG_OK && accept) {
                    apply_game(&scratch); view = V_GAME; next_poll = now + pdMS_TO_TICKS(5000);
                } else {
                    const char *msg = NULL;
                    if (r == EG_CONFLICT) msg = "That challenge isn't open anymore.";
                    else if (r == EG_NO_GAME) msg = "Accepted. Open the game from Your games.";
                    else if (r == EG_ERROR) msg = "Couldn't answer the challenge. Try again.";
                    if (msg) { lvgl_port_lock(0); eg_home_set_status(scr_home, msg); lvgl_port_unlock(); }
                    next_poll = msg ? now + pdMS_TO_TICKS(4000) : 0;   /* let the message show, then refresh the list */
                }
                if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }
            }
            if (req.set) {
                req.set = false;
                r = eg_api_set(token, req.set_id);
                if (r == EG_OK) {
                    snprintf(home.active_set, sizeof home.active_set, "%s", req.set_id);
                    eg_pieces_load(token, &changed);      /* new set's pieces for every board */
                }
            }
            if (req.rank) {
                req.rank = false;
                r = eg_api_leaderboard(token, &rank);
                lvgl_port_lock(0);
                eg_rank_set(scr_rank, &rank, r == EG_OK ? NULL : "Couldn't load the leaderboard.");
                lvgl_port_unlock();
            }
            if (req.thumbs) {
                req.thumbs = false;
                bool any = false;
                for (int i = 0; i < home.n_sets; i++) {
                    if (!eg_thumb_find(home.sets[i].preview_k) && eg_thumb_load(home.sets[i].preview_k)) any = true;
                    if (!eg_thumb_find(home.sets[i].preview_n) && eg_thumb_load(home.sets[i].preview_n)) any = true;
                }
                if (any) { lvgl_port_lock(0); eg_sets_set(scr_sets, &home); lvgl_port_unlock(); }
            }
            if (req.resign && view == V_GAME && have_game) {
                req.resign = false;
                r = eg_api_resign(token, cur.id, &scratch);
                if (r == EG_OK) apply_game(&scratch);
            }
            if (mailbox.pending && view == V_GAME && have_game) {
                mailbox.pending = false;
                r = eg_api_move(token, &cur, mailbox.from, mailbox.to, mailbox.promo, &scratch);
                if (r != EG_ERROR && r != EG_UNAUTHORIZED) apply_game(&scratch);   /* 200, 409 and 422 carry the game */
                else if (r == EG_ERROR && eg_api_game_id(token, cur.id, &scratch) == EG_OK) apply_game(&scratch);   /* undo the optimistic move */
                next_poll = now + pdMS_TO_TICKS(3000);   /* computer replies show up fast */
            }
            if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }

            /* periodic refresh for whatever is on screen */
            if ((int32_t)(now - next_poll) >= 0) {
                if (view == V_GAME && have_game) {
                    r = eg_api_game_id(token, cur.id, &scratch);
                    if (r == EG_OK && (scratch.move_count != cur.move_count || strcmp(scratch.status, cur.status) != 0)) apply_game(&scratch);
                    next_poll = now + pdMS_TO_TICKS(cur.your_turn || !cur.in_progress ? 15000 : 5000);
                } else {
                    r = eg_api_home(token, &home);
                    lvgl_port_lock(0);
                    if (r == EG_OK) { eg_home_set(scr_home, &home); eg_you_set(scr_you, &home); }
                    else if (r != EG_UNAUTHORIZED) eg_home_set_status(scr_home, "Couldn't load your games. Retrying...");
                    lvgl_port_unlock();
                    if (view == V_CHAL && r == EG_OK && chal_seen_n >= 0) {
                        /* someone accepted: open the game that wasn't there when the link was made */
                        for (int i = 0; i < home.n_games; i++) {
                            bool seen = false;
                            for (int k = 0; k < chal_seen_n; k++) if (strcmp(chal_seen[k], home.games[i].id) == 0) seen = true;
                            if (!seen) {
                                snprintf(req.game_id, sizeof req.game_id, "%s", home.games[i].id);
                                req.open = true;
                                chal_seen_n = -1;
                                break;
                            }
                        }
                    }
                    if (view == V_HOME && xTaskGetTickCount() - last_update_check > pdMS_TO_TICKS(UPDATE_CHECK_MS) && !wifi_open) {
                        last_update_check = xTaskGetTickCount();   /* updates only from the home screen */
                        eg_ota_check_and_update(show_updating);
                        show(scr_home);
                    }
                    next_poll = now + pdMS_TO_TICKS(view == V_CHAL ? 8000 : 30000);
                }
                if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }
            }
            vTaskDelay(pdMS_TO_TICKS(250));
        }
        ESP_LOGW(TAG, "401: wiping token and re-pairing");
        eg_store_clear_token();
        token[0] = 0;
        have_game = false;
    }
}

void app_main(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }

    if (!eg_bsp_init()) { ESP_LOGE(TAG, "display init failed"); return; }

    lvgl_port_lock(0);
    scr_wifi = eg_wifi_create(NULL, on_wifi_connect, request_scan, on_wifi_back);
    eg_screens_set_wifi_handler(on_menu_wifi);
    scr_pair = eg_pair_create(NULL);
    eg_screens_set_tab_handler(on_tab);
    eg_home_cb_t hcb = {.on_play_ai = on_home_play_ai, .on_challenge = on_home_challenge, .on_open_game = on_home_open,
                        .on_quick_match = on_home_quick, .on_find_player = on_home_find, .on_respond = on_respond};
    scr_home = eg_home_create(NULL, &hcb);
    scr_ai = eg_ai_setup_create(NULL, on_ai_start, on_sub_back);
    scr_chal = eg_challenge_create(NULL, on_sub_back);
    scr_qm = eg_qm_create(NULL, on_qm_cancel, on_qm_retry);
    eg_find_cb_t fcb = {.on_query = on_find_query, .on_send = on_find_send, .on_link = on_home_challenge, .on_back = on_sub_back};
    scr_find = eg_find_create(NULL, &fcb);
    scr_sets = eg_sets_create(NULL, on_pick_set);
    scr_make = eg_make_create(NULL);
    scr_rank = eg_rank_create(NULL);
    scr_you = eg_you_create(NULL, on_forget, eg_ota_version());
    eg_game_cb_t cb = {.on_move = on_move, .on_resign = on_resign, .on_menu_home = on_menu_home};
    scr_game = eg_game_create(NULL, &cb);
    lvgl_port_unlock();
    xTaskCreate(scan_task, "scan", 10 * 1024, NULL, 4, &scan_task_h);
    xTaskCreate(net_task, "net", 16 * 1024, NULL, 5, NULL);
}
