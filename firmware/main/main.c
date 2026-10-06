/* SPDX-License-Identifier: MIT */
/* Endgames Board firmware: boot, Wi-Fi, pairing, then play.
 * One network task owns all HTTP. The UI runs in LVGL's task; every LVGL call from the network
 * task is wrapped in lvgl_port_lock / unlock. */
#include "api.h"
#include "bsp.h"
#include "esp_heap_caps.h"
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
static lv_obj_t *scr_boot, *scr_wifi, *scr_pair, *scr_home, *scr_ai, *scr_chal, *scr_qm, *scr_find, *scr_sets, *scr_make, *scr_rank, *scr_you, *scr_game;
static eg_rank_t rank;
static eg_home_t home;                  /* last board-home result */
static eg_game_t cur;
static bool have_game;
static volatile bool want_wifi_screen, want_wifi_back;


static void boot_status(const char *msg)
{
    lvgl_port_lock(0);
    if (scr_boot && lv_screen_active() == scr_boot) eg_boot_set_status(scr_boot, msg);
    lvgl_port_unlock();
}

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
    volatile bool home, ai, challenge, open, set, resign, rank, thumbs, sets, set_edit, poll_now;
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
static char qm_tc[8] = "10+0";            /* the clock picked for quick match, "" untimed */
static void on_home_quick(void)
{
    eg_qm_pick(scr_qm);
    ui_load(scr_qm);
}
static void on_qm_join(const char *tc)
{
    snprintf(qm_tc, sizeof qm_tc, "%s", tc ? tc : "");
    eg_qm_set(scr_qm, "Looking for an opponent...", true);
    req.qm_join = true;
}
static void on_qm_cancel(void)
{
    req.qm_join = false;
    req.qm_cancel = true;
    behind = scr_home;
    ui_load(scr_home);
}
static void on_qm_retry(void) { on_qm_join(qm_tc); }

/* Challenge a player */
static void on_home_find(void) { eg_find_reset(scr_find); ui_load(scr_find); }
static void on_find_query(const char *q)
{
    snprintf(req.query, sizeof req.query, "%s", q);
    req.search_at = xTaskGetTickCount() + pdMS_TO_TICKS(400);
    req.search = strlen(req.query) >= 3;
}
static char chal_tc[8];
static void on_find_send(const char *id, const char *name, const char *first, const char *time_control)
{
    snprintf(chal_tc, sizeof chal_tc, "%s", time_control ? time_control : "");
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
    lv_obj_t *screens[4] = {scr_home, scr_sets, scr_rank, scr_you};
    if (tab < 0 || tab > 3) return;
    if (tab == 1) { eg_sets_set(scr_sets, &home); req.sets = true; }
    if (tab == 2) req.rank = true;
    if (tab == 3) eg_you_set(scr_you, &home);
    if (tab == 0) req.home = true;
    behind = screens[tab];                 /* Wi-Fi's Back returns here */
    ui_load(screens[tab]);
}
static void on_make(void) { ui_load(scr_make); }
static void on_make_back(void) { req.sets = true; behind = scr_sets; ui_load(scr_sets); }   /* a new set may be ready */
static void on_home_open(const char *id) { snprintf(req.game_id, sizeof req.game_id, "%s", id); req.open = true; }
static void on_sub_back(void) { req.search = false; req.home = true; behind = scr_home; ui_load(scr_home); }
static void on_chal_back(void) { ui_load(scr_find); }   /* Send a link was opened from Challenge a player */
static void on_ai_start(const char *difficulty, const char *color)
{
    snprintf(req.difficulty, sizeof req.difficulty, "%s", difficulty);
    snprintf(req.color, sizeof req.color, "%s", color);
    req.ai = true;
}
static void on_pick_set(const char *id) { snprintf(req.set_id, sizeof req.set_id, "%s", id); req.set = true; }
static char set_edit_id[48], set_edit_name[40];
static bool set_edit_delete;
static void on_set_rename(const char *id, const char *name)
{
    snprintf(set_edit_id, sizeof set_edit_id, "%s", id);
    snprintf(set_edit_name, sizeof set_edit_name, "%s", name);
    set_edit_delete = false;
    req.set_edit = true;
}
static void on_set_delete(const char *id)
{
    snprintf(set_edit_id, sizeof set_edit_id, "%s", id);
    set_edit_delete = true;
    req.set_edit = true;
}
static void on_clock_zero(void) { req.poll_now = true; }
static void on_menu_home(void) { req.home = true; ui_load(scr_home); }
static void on_resign(void) { req.resign = true; }

/* ---------- power and notifications (UI task) ----------
 * Dim to 20% after 2 minutes untouched, backlight off after 10. The first touch on a dark screen only
 * wakes it. Never dark while it's your move in a timed game. No speaker, so notifications are visual:
 * a dot on Play, a banner at the top, and a dark screen lights up for 60 s. */

#define DIM_MS (2 * 60 * 1000)
#define OFF_MS (10 * 60 * 1000)
#define NOTIFY_AWAKE_MS (60 * 1000)

static volatile bool keep_awake;          /* set by the network task */
static int bl_now = -1;
static lv_obj_t *catcher, *banner;
static bool notify_woke;
static char banner_game[40];
static bool banner_is_challenge;

static void backlight(int pct)
{
    if (pct == bl_now) return;
    bl_now = pct;
    eg_bsp_backlight((uint8_t)pct);
}

static void catcher_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) { backlight(100); notify_woke = false; }
    else if (c == LV_EVENT_CLICKED || c == LV_EVENT_PRESS_LOST) {
        lv_obj_delete_async(catcher);
        catcher = NULL;
    }
}

static void go_dark(void)
{
    backlight(0);
    notify_woke = false;
    if (catcher) return;
    catcher = lv_obj_create(lv_layer_top());           /* eats the waking touch */
    lv_obj_remove_style_all(catcher);
    lv_obj_set_size(catcher, 800, 480);
    lv_obj_add_flag(catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(catcher, catcher_cb, LV_EVENT_ALL, NULL);
}

static void power_tick(lv_timer_t *t)
{
    (void)t;
    uint32_t idle = lv_display_get_inactive_time(NULL);
    if (catcher && bl_now == 0) {                       /* dark: stay dark until touched or notified */
        if (keep_awake) { lv_obj_delete(catcher); catcher = NULL; backlight(100); }
        return;
    }
    if (notify_woke) {
        if (idle < 1000) notify_woke = false;           /* someone touched it: normal rules again */
        else if (idle > NOTIFY_AWAKE_MS) { go_dark(); return; }
        else { backlight(100); return; }
    }
    if (keep_awake || idle < DIM_MS) backlight(100);
    else if (idle < OFF_MS) backlight(20);
    else go_dark();
}

static void banner_close(void)
{
    if (banner) { lv_obj_delete(banner); banner = NULL; }
}
static void banner_x_cb(lv_event_t *e) { (void)e; banner_close(); }
static void banner_open_cb(lv_event_t *e)
{
    (void)e;
    if (banner_is_challenge || !banner_game[0]) { req.home = true; behind = scr_home; ui_load(scr_home); }
    else { snprintf(req.game_id, sizeof req.game_id, "%s", banner_game); req.open = true; }
    banner_close();
}
static void banner_timeout(lv_timer_t *t) { (void)t; banner_close(); }
static void banner_y(void *o, int32_t v) { lv_obj_set_y(o, v); }

/* Call with the LVGL lock held. */
static void notify(const char *text, const char *game_id, bool challenge)
{
    banner_close();
    snprintf(banner_game, sizeof banner_game, "%s", game_id ? game_id : "");
    banner_is_challenge = challenge;
    banner = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(banner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(banner, 800, 64);
    lv_obj_set_pos(banner, 0, -64);
    lv_obj_set_style_bg_color(banner, EG_BG_VOID, 0);
    lv_obj_set_style_radius(banner, 0, 0);
    lv_obj_set_style_border_width(banner, 3, 0);
    lv_obj_set_style_border_side(banner, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(banner, challenge ? EG_CYAN : EG_PINK, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);
    lv_obj_t *dot = lv_obj_create(banner);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 14, 14);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, challenge ? EG_CYAN : EG_PINK, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 24, 0);
    lv_obj_t *l = lv_label_create(banner);
    lv_obj_set_style_text_font(l, &eg_sora_20_bold, 0);
    lv_obj_set_style_text_color(l, EG_FG, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, 560);
    lv_label_set_text(l, text);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 52, 0);
    lv_obj_t *hint = lv_label_create(banner);
    lv_obj_set_style_text_font(hint, &eg_sora_16, 0);
    lv_obj_set_style_text_color(hint, EG_FG_HAZE, 0);
    lv_label_set_text(hint, "Tap to open");
    lv_obj_align(hint, LV_ALIGN_RIGHT_MID, -84, 0);
    lv_obj_t *x = lv_button_create(banner);
    lv_obj_set_size(x, 56, 44);
    lv_obj_align(x, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_bg_color(x, EG_SURFACE, 0);
    lv_obj_set_style_radius(x, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(x, 0, 0);
    lv_obj_t *xl = lv_label_create(x);
    lv_obj_set_style_text_font(xl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(xl, EG_FG, 0);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_center(xl);
    lv_obj_add_event_cb(x, banner_x_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(banner, banner_open_cb, LV_EVENT_CLICKED, NULL);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, banner);
    lv_anim_set_exec_cb(&a, banner_y);
    lv_anim_set_values(&a, -64, 0);
    lv_anim_set_duration(&a, 260);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    lv_timer_t *t = lv_timer_create(banner_timeout, NOTIFY_AWAKE_MS, NULL);
    lv_timer_set_repeat_count(t, 1);
    if (catcher) { lv_obj_delete(catcher); catcher = NULL; }   /* a dark screen lights up */
    if (bl_now != 100) { backlight(100); notify_woke = true; }
}

/* What's waiting for you (games against people where it's your move, and challenges), compared
 * with the last home refresh. Network task. */
static char seen[EG_HOME_MAX_GAMES + EG_HOME_MAX_INCOMING][40];
static int seen_n = -1;                    /* -1: first refresh after boot, nothing is "new" */

static void check_waiting(const eg_home_t *h)
{
    char now_ids[EG_HOME_MAX_GAMES + EG_HOME_MAX_INCOMING][40];
    int n = 0;
    const char *new_text = NULL, *new_game = NULL;
    bool new_chal = false;
    static char text[96];
    int new_count = 0;
    for (int i = 0; i < h->n_incoming; i++) snprintf(now_ids[n++], 40, "%s", h->incoming[i].id);
    for (int i = 0; i < h->n_games; i++)
        if (h->games[i].your_turn && !h->games[i].opponent_ai) snprintf(now_ids[n++], 40, "%s", h->games[i].id);
    if (seen_n >= 0) {
        for (int i = 0; i < n; i++) {
            bool was = false;
            for (int k = 0; k < seen_n; k++) if (strcmp(seen[k], now_ids[i]) == 0) was = true;
            if (was) continue;
            new_count++;
            if (new_text) continue;
            if (i < h->n_incoming) {
                const eg_incoming_t *c = &h->incoming[i];
                snprintf(text, sizeof text, "%s challenged you", c->name[0] ? c->name : "Someone");
                new_chal = true;
            } else {
                for (int g = 0; g < h->n_games; g++)
                    if (strcmp(h->games[g].id, now_ids[i]) == 0) {
                        snprintf(text, sizeof text, "Your move against %s", h->games[g].opponent[0] ? h->games[g].opponent : "your opponent");
                        new_game = h->games[g].id;
                    }
            }
            new_text = text;
        }
    }
    if (new_count > 1) snprintf(text, sizeof text, "%d games and challenges are waiting for you", new_count);
    memcpy(seen, now_ids, sizeof now_ids[0] * n);
    seen_n = n;
    lvgl_port_lock(0);
    eg_screens_set_badge(n > 0);
    if (new_text) notify(text, new_count > 1 ? NULL : new_game, new_count > 1 || new_chal);
    lvgl_port_unlock();
}

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
    boot_status("Connecting to Wi-Fi...");
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
    boot_status("Checking for updates...");
    eg_ota_check_and_update(show_updating);     /* restarts if there is a newer one */
    TickType_t last_update_check = xTaskGetTickCount();
    eg_store_load_token(token);
    for (;;) {
        if (!token[0]) {
            while (!pair()) { /* expired: loop with a new code */ }
        }
        bool changed;
        boot_status("Loading your pieces...");
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
                    r = eg_api_quick_match(token, "cancel", NULL, &qs, &scratch);
                    if (r == EG_OK) {
                        /* cancel only removes an unmatched entry; a match that landed first still opens */
                        r = eg_api_quick_match(token, NULL, NULL, &qs, &scratch);
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
                r = eg_api_quick_match(token, "join", qm_tc, &qs, &scratch);
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
                r = eg_api_quick_match(token, NULL, NULL, &qs, &scratch);
                uint32_t secs = (now - qm_started) / configTICK_RATE_HZ;
                if (r == EG_OK && qs == EG_QM_MATCHED) {
                    apply_game(&scratch); view = V_GAME; next_poll = now + pdMS_TO_TICKS(5000);
                } else if (r == EG_NO_GAME) {
                    view = V_HOME; next_poll = 0;
                    lvgl_port_lock(0); eg_qm_set(scr_qm, "Matched! Open the game from Your games.", false); lvgl_port_unlock();
                } else if ((r == EG_OK && qs == EG_QM_IDLE) || secs >= 5 * 60) {
                    /* the server drops entries after 5 minutes; stop on our side too */
                    if (secs >= 5 * 60) eg_api_quick_match(token, "cancel", NULL, &qs, &scratch);
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
                r = eg_api_challenge_player(token, req.opp_id, req.first, chal_tc);
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
                    req.thumbs = true;                    /* redraw Sets with the new pick marked */
                }
            }
            if (req.rank) {
                req.rank = false;
                r = eg_api_leaderboard(token, &rank);
                lvgl_port_lock(0);
                eg_rank_set(scr_rank, &rank, r == EG_OK ? NULL : "Couldn't load the leaderboard.");
                lvgl_port_unlock();
            }
            if (req.set_edit) {
                req.set_edit = false;
                r = eg_api_set_edit(token, set_edit_id, set_edit_delete ? NULL : set_edit_name);
                lvgl_port_lock(0);
                eg_sets_edit_done(scr_sets, r == EG_OK,
                                  r == EG_CONFLICT ? "That set is gone. It may have been deleted on the web."
                                                   : "Couldn't save. Check Wi-Fi and try again.");
                lvgl_port_unlock();
                if (r == EG_OK) {
                    req.sets = true;                                  /* fresh list */
                    if (set_edit_delete) eg_pieces_load(token, &changed);   /* deleting the active set falls back to Default */
                }
            }
            if (req.sets) {                      /* Sets opened: fetch the list fresh (a set made on the phone shows up) */
                req.sets = false;
                if (eg_api_home(token, &home) == EG_OK) {
                    lvgl_port_lock(0); eg_home_set(scr_home, &home); eg_you_set(scr_you, &home); lvgl_port_unlock();
                }
                req.thumbs = true;
            }
            if (req.thumbs) {
                req.thumbs = false;
                lvgl_port_lock(0); eg_sets_set(scr_sets, &home); lvgl_port_unlock();   /* names first */
                int fresh = 0;
                for (int i = 0; i < home.n_sets; i++) {
                    if (!eg_thumb_find(home.sets[i].preview_k) && eg_thumb_load(home.sets[i].preview_k)) fresh++;
                    if (!eg_thumb_find(home.sets[i].preview_n) && eg_thumb_load(home.sets[i].preview_n)) fresh++;
                    if (fresh >= 4) { fresh = 0; lvgl_port_lock(0); eg_sets_set(scr_sets, &home); lvgl_port_unlock(); }
                }
                lvgl_port_lock(0); eg_sets_set(scr_sets, &home); lvgl_port_unlock();   /* the rest, and "Pieces missing" */
            }
            if (req.resign && view == V_GAME && have_game) {
                req.resign = false;
                r = eg_api_resign(token, cur.id, &scratch);
                if (r == EG_OK) {                         /* resigned: back to the home screen */
                    have_game = false;
                    view = V_HOME;
                    next_poll = 0;
                    show_main(scr_home);
                }
            }
            if (mailbox.pending && view == V_GAME && have_game) {
                mailbox.pending = false;
                r = eg_api_move(token, &cur, mailbox.from, mailbox.to, mailbox.promo, &scratch);
                if (r != EG_ERROR && r != EG_UNAUTHORIZED) apply_game(&scratch);   /* 200, 409 and 422 carry the game */
                else if (r == EG_ERROR && eg_api_game_id(token, cur.id, &scratch) == EG_OK) apply_game(&scratch);   /* undo the optimistic move */
                next_poll = now + pdMS_TO_TICKS(3000);   /* computer replies show up fast */
            }
            if (r == EG_UNAUTHORIZED) { unauthorized = true; break; }

            if (req.poll_now) { req.poll_now = false; next_poll = now; }   /* a clock ran out: let the server settle it */

            /* periodic refresh for whatever is on screen */
            if ((int32_t)(now - next_poll) >= 0) {
                if (view == V_GAME && have_game) {
                    r = eg_api_game_id(token, cur.id, &scratch);
                    if (r == EG_OK && (scratch.move_count != cur.move_count || strcmp(scratch.status, cur.status) != 0)) apply_game(&scratch);
                    else if (r == EG_OK && scratch.timed) {        /* same position: just correct the clocks */
                        cur.you_ms = scratch.you_ms; cur.opp_ms = scratch.opp_ms; cur.running = scratch.running;
                        lvgl_port_lock(0); eg_game_clock(scr_game, &scratch); lvgl_port_unlock();
                    }
                    next_poll = now + pdMS_TO_TICKS(cur.timed && cur.in_progress ? 5000 : cur.your_turn || !cur.in_progress ? 15000 : 5000);
                } else {
                    r = eg_api_home(token, &home);
                    lvgl_port_lock(0);
                    if (r == EG_OK) { eg_home_set(scr_home, &home); eg_you_set(scr_you, &home); }
                    else if (r != EG_UNAUTHORIZED) eg_home_set_status(scr_home, "Couldn't load your games. Retrying...");
                    lvgl_port_unlock();
                    if (r == EG_OK) check_waiting(&home);
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
            keep_awake = view == V_GAME && have_game && cur.in_progress && cur.timed && cur.your_turn;
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
    scr_boot = eg_boot_create(NULL);          /* something on screen right away */
    lv_screen_load(scr_boot);
    lvgl_port_unlock();

    /* LVGL's 256 KB pool is too small for every screen plus piece decoding: give it 8 MB of PSRAM. */
    {
        const size_t extra = 8 * 1024 * 1024;
        void *pool = heap_caps_malloc(extra, MALLOC_CAP_SPIRAM);
        lvgl_port_lock(0);
        if (!pool) ESP_LOGW(TAG, "no PSRAM for the LVGL pool");
        else if (!lv_mem_add_pool(pool, extra)) { ESP_LOGE(TAG, "LVGL refused the extra pool"); heap_caps_free(pool); }
        else ESP_LOGI(TAG, "LVGL pool +%u KB", (unsigned)(extra / 1024));
        lvgl_port_unlock();
    }

    lvgl_port_lock(0);
    scr_wifi = eg_wifi_create(NULL, on_wifi_connect, request_scan, on_wifi_back);
    eg_screens_set_wifi_handler(on_menu_wifi);
    scr_pair = eg_pair_create(NULL);
    eg_screens_set_tab_handler(on_tab);
    eg_home_cb_t hcb = {.on_play_ai = on_home_play_ai, .on_challenge = on_home_challenge, .on_open_game = on_home_open,
                        .on_quick_match = on_home_quick, .on_find_player = on_home_find, .on_respond = on_respond};
    scr_home = eg_home_create(NULL, &hcb);
    scr_ai = eg_ai_setup_create(NULL, on_ai_start, on_sub_back);
    scr_chal = eg_challenge_create(NULL, on_chal_back);
    scr_qm = eg_qm_create(NULL, on_qm_join, on_qm_cancel, on_qm_retry);
    eg_find_cb_t fcb = {.on_query = on_find_query, .on_send = on_find_send, .on_link = on_home_challenge, .on_back = on_sub_back};
    scr_find = eg_find_create(NULL, &fcb);
    scr_sets = eg_sets_create(NULL, on_pick_set, on_make);
    scr_make = eg_make_create(NULL, on_make_back);
    eg_sets_set_edit(scr_sets, on_set_rename, on_set_delete);
    scr_rank = eg_rank_create(NULL);
    scr_you = eg_you_create(NULL, on_forget, eg_ota_version());
    eg_game_cb_t cb = {.on_move = on_move, .on_resign = on_resign, .on_menu_home = on_menu_home, .on_clock_zero = on_clock_zero};
    scr_game = eg_game_create(NULL, &cb);
    lv_timer_create(power_tick, 500, NULL);
    lvgl_port_unlock();
    xTaskCreate(scan_task, "scan", 10 * 1024, NULL, 4, &scan_task_h);
    xTaskCreate(net_task, "net", 16 * 1024, NULL, 5, NULL);
}
