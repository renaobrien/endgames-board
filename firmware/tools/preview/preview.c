/* SPDX-License-Identifier: MIT */
/* Renders the game screen headlessly at 800x480 and writes a BMP. render.sh turns it into a PNG. */
#include "chess_pos.h"
#include "lvgl.h"
#include "ui_game.h"
#include "ui_screens.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define W 800
#define H 480

static uint32_t fb[W * H];

static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++)
        memcpy(&fb[y * W + a->x1], px + (size_t)(y - a->y1) * w * 4, (size_t)w * 4);
    lv_display_flush_ready(d);
}

static void write_bmp(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    uint32_t size = 54 + W * H * 4;
    uint8_t h[54] = {'B', 'M'};
    memcpy(h + 2, &size, 4);
    uint32_t off = 54, dib = 40, w = W, hh = (uint32_t)(-H), planes_bpp = 1 | (32 << 16);
    memcpy(h + 10, &off, 4); memcpy(h + 14, &dib, 4); memcpy(h + 18, &w, 4);
    memcpy(h + 22, &hh, 4);  memcpy(h + 26, &planes_bpp, 4);
    fwrite(h, 1, 54, f);
    fwrite(fb, 4, W * H, f);
    fclose(f);
}

static void nop_move(const char *a, const char *b, char p) { (void)a; (void)b; (void)p; }

/* /* A sample position after 1.e4 e5 2.Nf3 Nc6 3.Bc4 Nf6 4.Ng5 d5 5.exd5 Nxd5 6.Nxf7 */
static void sample(eg_game_t *g)
{
    memset(g, 0, sizeof *g);
    strcpy(g->id, "sample");
    eg_parse_fen(g, "r1bqkb1r/ppp2Npp/2n5/3np3/2B5/8/PPPP1PPP/RNBQK2R b KQkq - 0 6");
    g->you_white = true;
    g->in_progress = true;
    strcpy(g->status, "IN_PROGRESS");
    g->your_turn = false;
    g->move_count = 11;
    strcpy(g->last_from, "g5");
    strcpy(g->last_to, "f7");
    strcpy(g->opponent, "magnus_fan");
    static const char *san[] = {"e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6", "Ng5", "d5", "exd5", "Nxd5", "Nxf7"};
    g->moves_n = 11;
    for (int i = 0; i < 11; i++) strcpy(g->moves[i], san[i]);
    g->clock_you_s = 8 * 60 + 41;   /* sample clocks: Endgames has no clocks yet, shown to preview the type */
    g->clock_opp_s = 7 * 60 + 12;
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "game.bmp";
    lv_init();
    lv_display_t *d = lv_display_create(W, H);
    static uint8_t buf[W * H * 4];
    lv_display_set_buffers(d, buf, NULL, sizeof buf, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, flush);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_ARGB8888);

    const char *which = argc > 2 ? argv[2] : "game";
    if (strcmp(which, "pair") == 0) {
        lv_obj_t *p = eg_pair_create(lv_screen_active());
        eg_pair_set_code(p, "K7XQ2M", "endgam.es/board/pair");
    } else if (strcmp(which, "wifi") == 0) {
        lv_obj_t *w = eg_wifi_create(lv_screen_active(), NULL, NULL);
        eg_ap_t aps[] = {{"Renas Wifi", -48, false}, {"NETGEAR-5G", -61, false}, {"xfinitywifi", -70, true}, {"Coffee Shop Guest Wifi", -82, false}};
        eg_wifi_set_networks(w, aps, 4);
        if (argc > 3) lv_dropdown_open(lv_obj_get_child(w, 1));
    } else if (strcmp(which, "idle") == 0) {
        eg_idle_create(lv_screen_active());
    } else {
        eg_game_cb_t cb = {.on_move = nop_move};
        lv_obj_t *scr = eg_game_create(lv_screen_active(), &cb);
        eg_game_t g;
        sample(&g);
        eg_game_set(scr, &g);
    }

    for (int i = 0; i < 5; i++) { lv_tick_inc(50); lv_timer_handler(); }
    write_bmp(out);
    printf("wrote %s\n", out);
    return 0;
}
