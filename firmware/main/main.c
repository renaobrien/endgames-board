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
static lv_obj_t *scr_wifi, *scr_pair, *scr_idle, *scr_game;
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

        int idle_ms = 0;
        for (;;) {
            handle_wifi();
            if (!have_game && xTaskGetTickCount() - last_update_check > pdMS_TO_TICKS(UPDATE_CHECK_MS) && !wifi_open) {
                last_update_check = xTaskGetTickCount();   /* only between games */
                if (!eg_ota_check_and_update(show_updating) && token[0]) show_main(scr_idle);
            }
            if (mailbox.pending && have_game) {
                mailbox.pending = false;
                eg_game_t next;
                eg_result_t r = eg_api_move(token, &cur, mailbox.from, mailbox.to, mailbox.promo, &next);
                if (r == EG_UNAUTHORIZED) break;
                if (r != EG_ERROR) apply_game(&next);   /* 200, 409 and 422 all carry the current game */
            }
            eg_game_t g;
            eg_result_t r = eg_api_game(token, &g);
            if (r == EG_UNAUTHORIZED) break;
            if (r == EG_OK) {
                if (!have_game || g.move_count != cur.move_count || strcmp(g.status, cur.status) != 0 || strcmp(g.id, cur.id) != 0)
                    apply_game(&g);
                idle_ms = 10000;                         /* in a game: 10 s */
            } else if (r == EG_NO_GAME) {
                have_game = false;
                if (behind != scr_idle) show_main(scr_idle);   /* also right after pairing */
                idle_ms = 60000;                         /* idle: 60 s */
            } else {
                idle_ms = 10000;
            }
            for (int waited = 0; waited < idle_ms; waited += 250) {
                vTaskDelay(pdMS_TO_TICKS(250));
                handle_wifi();
                if (mailbox.pending) break;   /* react to moves quickly */
            }
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
    scr_idle = eg_idle_create(NULL);
    eg_game_cb_t cb = {.on_move = on_move, .on_menu_forget = on_forget, .on_menu_wifi = on_menu_wifi};
    scr_game = eg_game_create(NULL, &cb);
    lvgl_port_unlock();
    xTaskCreate(scan_task, "scan", 10 * 1024, NULL, 4, &scan_task_h);
    xTaskCreate(net_task, "net", 12 * 1024, NULL, 5, NULL);
}
