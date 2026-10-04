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
static volatile bool want_wifi_screen;

static void show(lv_obj_t *s)
{
    lvgl_port_lock(0);
    lv_screen_load(s);
    lvgl_port_unlock();
}

/* ---------- UI callbacks (run in LVGL task) ---------- */

static void on_wifi_connect(const char *ssid, const char *pass)
{
    eg_store_save_wifi(ssid, pass);
    esp_restart();                      /* simplest clean reconnect */
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
    lv_screen_load(scr_game);
    lvgl_port_unlock();
}

static bool pair(void)
{
    eg_pairing_t p;
    show(scr_pair);
    while (eg_api_pair_start(&p) != EG_OK) vTaskDelay(pdMS_TO_TICKS(5000));
    lvgl_port_lock(0);
    eg_pair_set_code(scr_pair, p.code, p.claim_url[0] ? p.claim_url : NULL);
    lvgl_port_unlock();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        eg_result_t r = eg_api_pair_poll(p.poll_secret, token);
        if (r == EG_OK) { eg_store_save_token(token); return true; }
        if (r == EG_EXPIRED) return false;      /* caller starts over with a fresh code */
    }
}

static void net_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!token[0]) {
            while (!pair()) { /* expired: loop with a new code */ }
        }
        bool changed;
        eg_pieces_load(token, &changed);

        int idle_ms = 0;
        for (;;) {
            if (want_wifi_screen) { want_wifi_screen = false; show(scr_wifi); }
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
                if (have_game) { have_game = false; show(scr_idle); }
                idle_ms = 60000;                         /* idle: 60 s */
            } else {
                idle_ms = 10000;
            }
            for (int waited = 0; waited < idle_ms; waited += 250) {
                vTaskDelay(pdMS_TO_TICKS(250));
                if (mailbox.pending || want_wifi_screen) break;   /* react to taps quickly */
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
    scr_wifi = eg_wifi_create(NULL, on_wifi_connect);
    scr_pair = eg_pair_create(NULL);
    scr_idle = eg_idle_create(NULL);
    eg_game_cb_t cb = {.on_move = on_move, .on_menu_forget = on_forget, .on_menu_wifi = on_menu_wifi};
    scr_game = eg_game_create(NULL, &cb);
    lvgl_port_unlock();

    char ssid[33] = {0}, pass[65] = {0};
#ifdef EG_DEV_WIFI_SSID
    if (!eg_store_load_wifi(ssid, pass) && EG_DEV_WIFI_SSID[0]) { strcpy(ssid, EG_DEV_WIFI_SSID); strcpy(pass, EG_DEV_WIFI_PASS); }
#else
    eg_store_load_wifi(ssid, pass);
#endif
    if (!ssid[0] || !eg_bsp_wifi_connect(ssid, pass, 20000)) {
        show(scr_wifi);
        if (ssid[0]) { lvgl_port_lock(0); eg_wifi_set_error(scr_wifi, "Could not connect. Check the name and password."); lvgl_port_unlock(); }
        return;                         /* on_wifi_connect saves and restarts */
    }

    eg_store_load_token(token);
    xTaskCreate(net_task, "net", 12 * 1024, NULL, 5, NULL);
}
