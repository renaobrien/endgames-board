/* SPDX-License-Identifier: MIT */
/* Game screen: 480x480 board on the left (60 px squares), 320 px panel on the right.
 * Looks like the website's v2 game screen: sunset palette, pink/yellow last-move tint,
 * magenta check glow, mint legal-move dots, cyan edge. */
#include "ui_game.h"
#include "fonts.h"
#include "pieces.h"
#include "theme.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQ 60
#define BOARD 480
#define PANEL_W 320
#define PAD 16

typedef struct {
    eg_game_t g;
    eg_game_cb_t cb;
    bool flipped;            /* true: black at the bottom */
    int selected;            /* game square index, or -1 */
    char pending_from[3], pending_to[3];
    /* Everything per-square is indexed by screen cell (row*8+col). */
    lv_obj_t *cells[64], *tint[64], *piece[64], *mark[64];
    lv_obj_t *opp_name, *opp_clock, *you_name, *you_clock, *status, *list;
    lv_obj_t *btn_resign, *btn_flip, *btn_menu;
    lv_obj_t *promo, *menu;
} ui_t;

static ui_t *U(lv_obj_t *screen) { return (ui_t *)lv_obj_get_user_data(screen); }

/* Game square <-> screen cell. Flipping rotates the board 180 degrees, so the map is its own inverse. */
static int cell_of(const ui_t *u, int sq) { return u->flipped ? 63 - sq : sq; }

static void sq_name(int idx, char out[3])
{
    out[0] = (char)('a' + idx % 8);
    out[1] = (char)('0' + 8 - idx / 8);
    out[2] = 0;
}

static lv_obj_t *mk_label(lv_obj_t *p, const lv_font_t *f, lv_color_t c, const char *txt)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, txt);
    return l;
}

static void plain(lv_obj_t *o)
{
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static void fmt_clock(char *out, size_t n, int s)
{
    if (s < 0) snprintf(out, n, "--:--");
    else snprintf(out, n, "%d:%02d", s / 60, s % 60);
}

/* ---------- board ---------- */

static void clear_marks(ui_t *u)
{
    for (int i = 0; i < 64; i++) {
        lv_obj_add_flag(u->mark[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_width(u->mark[i], 0, 0);
        lv_obj_set_style_bg_opa(u->mark[i], LV_OPA_TRANSP, 0);
    }
}

static void show_targets(ui_t *u)
{
    clear_marks(u);
    if (u->selected < 0) return;
    char from[3];
    sq_name(u->selected, from);

    lv_obj_t *m = u->mark[cell_of(u, u->selected)];
    lv_obj_clear_flag(m, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(m, 3, 0);
    lv_obj_set_style_border_color(m, EG_BOARD_EDGE, 0);

    for (int i = 0; i < u->g.legal_n; i++) {
        if (strncmp(u->g.legal[i], from, 2) != 0) continue;
        int t = eg_sq_index(u->g.legal[i] + 2);
        if (t < 0) continue;
        lv_obj_t *d = u->mark[cell_of(u, t)];
        lv_obj_clear_flag(d, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        if (u->g.board[t]) {                      /* capture: ring */
            lv_obj_set_style_border_width(d, 3, 0);
            lv_obj_set_style_border_color(d, EG_MINT, 0);
        } else {                                  /* quiet move: dot */
            lv_obj_set_style_bg_color(d, EG_MINT, 0);
            lv_obj_set_style_bg_opa(d, LV_OPA_80, 0);
        }
    }
}

static void redraw_board(ui_t *u)
{
    int king = -1;
    bool check = u->g.in_progress && eg_in_check(&u->g, &king);
    int lf = eg_sq_index(u->g.last_from), lt = eg_sq_index(u->g.last_to);

    for (int cell = 0; cell < 64; cell++) {
        int idx = cell_of(u, cell);          /* same map both ways */
        bool dark = ((cell / 8) + (cell % 8)) % 2 == 1;
        lv_obj_set_style_bg_color(u->cells[cell], dark ? EG_BOARD_DARK : EG_BOARD_LIGHT, 0);

        lv_obj_t *t = u->tint[cell];
        if (check && idx == king) {
            lv_obj_set_style_bg_color(t, EG_MAGENTA, 0);
            lv_obj_set_style_bg_opa(t, LV_OPA_60, 0);
        } else if (idx == lf || idx == lt) {
            lv_obj_set_style_bg_color(t, dark ? EG_YELLOW : EG_PINK, 0);
            lv_obj_set_style_bg_opa(t, LV_OPA_30, 0);   /* web: 0.28 on dark, 0.32 on light */
        } else {
            lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
        }

        char c = u->g.board[idx];
        const void *src = c ? eg_piece_src(isupper((unsigned char)c) ? 'w' : 'b', (char)tolower((unsigned char)c)) : NULL;
        if (src) {
            lv_image_set_src(u->piece[cell], src);
            lv_obj_clear_flag(u->piece[cell], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(u->piece[cell], LV_OBJ_FLAG_HIDDEN);
        }
    }
    show_targets(u);
}

/* ---------- panel ---------- */

static void rebuild_moves(ui_t *u)
{
    lv_obj_clean(u->list);
    int n = u->g.moves_n;
    int start = n > 60 ? n - 60 : 0;
    if (start % 2) start--;                   /* begin on a white move */
    if (start < 0) start = 0;
    for (int i = start; i < n; i += 2) {
        lv_obj_t *row = lv_obj_create(u->list);
        plain(row);
        lv_obj_set_size(row, LV_PCT(100), 26);
        char num[8];
        snprintf(num, sizeof num, "%d.", (u->g.moves_first_ply + i) / 2 + 1);
        lv_obj_t *a = mk_label(row, &eg_vt323_36, EG_FG_MIST, num);
        lv_obj_align(a, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t *w = mk_label(row, &eg_sora_16, i == n - 1 ? EG_YELLOW : EG_FG, u->g.moves[i]);
        lv_obj_align(w, LV_ALIGN_LEFT_MID, 44, 0);
        if (i + 1 < n) {
            lv_obj_t *b = mk_label(row, &eg_sora_16, i + 1 == n - 1 ? EG_YELLOW : EG_FG, u->g.moves[i + 1]);
            lv_obj_align(b, LV_ALIGN_LEFT_MID, 150, 0);
        }
    }
    lv_obj_scroll_to_y(u->list, LV_COORD_MAX, LV_ANIM_OFF);
}

static void set_status(ui_t *u)
{
    char buf[64];
    lv_color_t col = EG_FG_HAZE;
    if (!u->g.in_progress) {
        if (strcmp(u->g.status, "DRAW") == 0) snprintf(buf, sizeof buf, "Draw");
        else if (strcmp(u->g.status, "ABANDONED") == 0) snprintf(buf, sizeof buf, "Game abandoned");
        else {
            bool white_won = strcmp(u->g.status, "WHITE_WON") == 0;
            bool you_won = white_won == u->g.you_white;
            snprintf(buf, sizeof buf, you_won ? "You won" : "You lost");
            col = you_won ? EG_MINT : EG_MAGENTA;
        }
    } else if (u->g.your_turn) {
        snprintf(buf, sizeof buf, "Your move");
        col = EG_CYAN;
    } else {
        snprintf(buf, sizeof buf, "Waiting for %.20s", u->g.opponent);
    }
    lv_label_set_text(u->status, buf);
    lv_obj_set_style_text_color(u->status, col, 0);
}

static void redraw_panel(ui_t *u)
{
    char buf[16];
    lv_label_set_text(u->opp_name, u->g.opponent);
    lv_label_set_text(u->you_name, "You");
    fmt_clock(buf, sizeof buf, u->g.clock_opp_s);
    lv_label_set_text(u->opp_clock, buf);
    fmt_clock(buf, sizeof buf, u->g.clock_you_s);
    lv_label_set_text(u->you_clock, buf);
    set_status(u);
    rebuild_moves(u);
    if (u->g.in_progress) lv_obj_clear_state(u->btn_resign, LV_STATE_DISABLED);
    else lv_obj_add_state(u->btn_resign, LV_STATE_DISABLED);
}

/* ---------- input ---------- */

static void close_overlay(lv_obj_t **o)
{
    if (*o) { lv_obj_delete(*o); *o = NULL; }
}

static void send_move(ui_t *u, char promo)
{
    if (u->cb.on_move) u->cb.on_move(u->pending_from, u->pending_to, promo);
    u->selected = -1;
    show_targets(u);
}

static void promo_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    char promo = (char)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    close_overlay(&u->promo);
    send_move(u, promo);
}

static void open_promotion(lv_obj_t *screen, ui_t *u)
{
    close_overlay(&u->promo);
    u->promo = lv_obj_create(screen);
    plain(u->promo);
    lv_obj_set_size(u->promo, BOARD, BOARD);
    lv_obj_set_pos(u->promo, 0, 0);
    lv_obj_set_style_bg_color(u->promo, EG_BG_VOID, 0);
    lv_obj_set_style_bg_opa(u->promo, LV_OPA_80, 0);
    lv_obj_t *title = mk_label(u->promo, &eg_bungee_28, EG_YELLOW, "PROMOTE TO");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 90);
    static const char kinds[4] = {'q', 'r', 'b', 'n'};
    char col = u->g.you_white ? 'w' : 'b';
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = lv_obj_create(u->promo);
        lv_obj_set_size(b, 96, 96);
        lv_obj_align(b, LV_ALIGN_CENTER, (i - 1) * 108 - 54, 20);
        lv_obj_set_style_bg_color(b, EG_SURFACE, 0);
        lv_obj_set_style_border_color(b, EG_CYAN, 0);
        lv_obj_set_style_border_width(b, 3, 0);
        lv_obj_set_style_radius(b, 18, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(b, (void *)(intptr_t)kinds[i]);
        const void *src = eg_piece_src(col, kinds[i]);
        if (src) {
            lv_obj_t *im = lv_image_create(b);
            lv_image_set_src(im, src);
            lv_image_set_scale(im, 256 * 80 / 60);
            lv_obj_center(im);
        }
        lv_obj_add_event_cb(b, promo_cb, LV_EVENT_CLICKED, screen);
    }
}

static void cell_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    int cell = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    int sq = cell_of(u, cell);
    if (!u->g.your_turn || !u->g.in_progress || u->promo) return;

    if (u->selected >= 0) {
        char from[3], to[3];
        sq_name(u->selected, from);
        sq_name(sq, to);
        bool promo = false;
        if (sq != u->selected && eg_move_legal(&u->g, from, to, &promo)) {
            memcpy(u->pending_from, from, 3);
            memcpy(u->pending_to, to, 3);
            if (promo) open_promotion(screen, u);
            else send_move(u, 0);
            return;
        }
    }
    if (eg_is_yours(&u->g, sq) && sq != u->selected) u->selected = sq;
    else u->selected = -1;
    show_targets(u);
}

static void flip_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    u->flipped = !u->flipped;
    u->selected = -1;
    redraw_board(u);
}

static void resign_cb(lv_event_t *e)
{
    ui_t *u = U(lv_event_get_user_data(e));
    if (u->cb.on_resign) u->cb.on_resign();
}

static void menu_item_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    int which = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    close_overlay(&u->menu);
    if (which == 1 && u->cb.on_menu_wifi) u->cb.on_menu_wifi();
    if (which == 2 && u->cb.on_menu_forget) u->cb.on_menu_forget();
}

static void menu_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    if (u->menu) { close_overlay(&u->menu); return; }
    u->menu = lv_obj_create(screen);
    plain(u->menu);
    lv_obj_set_size(u->menu, 240, 170);
    lv_obj_align(u->menu, LV_ALIGN_BOTTOM_RIGHT, -PAD, -84);
    lv_obj_set_style_bg_color(u->menu, EG_SURFACE, 0);
    lv_obj_set_style_bg_opa(u->menu, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(u->menu, 3, 0);
    lv_obj_set_style_border_color(u->menu, EG_PINK, 0);
    lv_obj_set_style_radius(u->menu, 18, 0);
    static const char *items[3] = {"Close", "Wi-Fi settings", "Forget this board"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_obj_create(u->menu);
        plain(b);
        lv_obj_set_size(b, 240, 56);
        lv_obj_set_pos(b, 0, 5 + i * 54);
        lv_obj_set_user_data(b, (void *)(intptr_t)i);
        lv_obj_t *l = mk_label(b, &eg_sora_16, i == 2 ? EG_PINK_SOFT : EG_FG, items[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, menu_item_cb, LV_EVENT_CLICKED, screen);
    }
}

/* ---------- build ---------- */

static lv_obj_t *mk_button(lv_obj_t *p, const char *txt, lv_color_t border, int x, int w)
{
    lv_obj_t *b = lv_obj_create(p);
    lv_obj_set_size(b, w, 56);
    lv_obj_set_pos(b, x, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, border, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_opa(b, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_t *l = mk_label(b, &eg_sora_16, border, txt);
    lv_obj_center(l);
    return b;
}

lv_obj_t *eg_game_create(lv_obj_t *parent, const eg_game_cb_t *cb)
{
    ui_t *u = calloc(1, sizeof *u);
    u->cb = *cb;
    u->selected = -1;

    lv_obj_t *scr = lv_obj_create(parent);
    plain(scr);
    lv_obj_set_size(scr, 800, 480);
    lv_obj_set_style_bg_color(scr, EG_BG_STAGE, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_user_data(scr, u);

    /* board */
    lv_obj_t *board = lv_obj_create(scr);
    plain(board);
    lv_obj_set_size(board, BOARD, BOARD);
    lv_obj_set_pos(board, 0, 0);
    for (int i = 0; i < 64; i++) {
        int sr = i / 8, sf = i % 8;
        lv_obj_t *c = lv_obj_create(board);
        plain(c);
        lv_obj_set_size(c, SQ, SQ);
        lv_obj_set_pos(c, sf * SQ, sr * SQ);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_user_data(c, (void *)(intptr_t)i);
        lv_obj_add_event_cb(c, cell_cb, LV_EVENT_CLICKED, scr);
        u->cells[i] = c;

        u->tint[i] = lv_obj_create(c);
        plain(u->tint[i]);
        lv_obj_set_size(u->tint[i], SQ, SQ);
        lv_obj_clear_flag(u->tint[i], LV_OBJ_FLAG_CLICKABLE);

        u->piece[i] = lv_image_create(c);
        lv_obj_set_size(u->piece[i], SQ, SQ);
        lv_obj_center(u->piece[i]);
        lv_obj_clear_flag(u->piece[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(u->piece[i], LV_OBJ_FLAG_HIDDEN);

        u->mark[i] = lv_obj_create(c);
        plain(u->mark[i]);
        lv_obj_set_size(u->mark[i], SQ - 6, SQ - 6);
        lv_obj_center(u->mark[i]);
        lv_obj_clear_flag(u->mark[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(u->mark[i], LV_OBJ_FLAG_HIDDEN);

        /* coordinates, like the website: file bottom-right on the bottom row, rank top-left on the left column */
        char t[2] = {0, 0};
        if (sr == 7) {
            t[0] = (char)('a' + sf);
            lv_obj_t *l = mk_label(c, &eg_sora_16, EG_PINK_SOFT, t);
            lv_obj_set_style_text_opa(l, LV_OPA_60, 0);
            lv_obj_align(l, LV_ALIGN_BOTTOM_RIGHT, -3, 0);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        }
        if (sf == 0) {
            t[0] = (char)('8' - sr);
            lv_obj_t *l = mk_label(c, &eg_sora_16, EG_PINK_SOFT, t);
            lv_obj_set_style_text_opa(l, LV_OPA_60, 0);
            lv_obj_align(l, LV_ALIGN_TOP_LEFT, 3, 0);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    /* panel */
    lv_obj_t *panel = lv_obj_create(scr);
    plain(panel);
    lv_obj_set_size(panel, PANEL_W, 480);
    lv_obj_set_pos(panel, BOARD, 0);
    lv_obj_set_style_bg_color(panel, EG_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_t *edge = lv_obj_create(panel);          /* cyan edge, pink offset (the website's board frame) */
    plain(edge);
    lv_obj_set_size(edge, 4, 480);
    lv_obj_set_style_bg_color(edge, EG_BOARD_EDGE, 0);
    lv_obj_set_style_bg_opa(edge, LV_OPA_COVER, 0);
    lv_obj_t *edge2 = lv_obj_create(panel);
    plain(edge2);
    lv_obj_set_size(edge2, 4, 480);
    lv_obj_set_pos(edge2, 4, 0);
    lv_obj_set_style_bg_color(edge2, EG_BOARD_SHADOW, 0);
    lv_obj_set_style_bg_opa(edge2, LV_OPA_COVER, 0);

    int cx = 8 + PAD;                         /* content x inside the panel */
    int cw = PANEL_W - cx - PAD;              /* content width */

    /* opponent strip */
    u->opp_name = mk_label(panel, &eg_sora_20_bold, EG_FG, "");
    lv_label_set_long_mode(u->opp_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(u->opp_name, cw - 110);
    lv_obj_set_pos(u->opp_name, cx, 22);
    u->opp_clock = mk_label(panel, &eg_vt323_56, EG_CYAN, "");
    lv_obj_align(u->opp_clock, LV_ALIGN_TOP_RIGHT, -PAD, 6);

    /* move list */
    u->list = lv_obj_create(panel);
    plain(u->list);
    lv_obj_set_size(u->list, cw, 236);
    lv_obj_set_pos(u->list, cx, 74);
    lv_obj_set_style_bg_color(u->list, EG_BG_VOID, 0);
    lv_obj_set_style_bg_opa(u->list, LV_OPA_50, 0);
    lv_obj_set_style_radius(u->list, 12, 0);
    lv_obj_set_style_pad_all(u->list, 8, 0);
    lv_obj_set_flex_flow(u->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(u->list, LV_OBJ_FLAG_SCROLLABLE);

    u->status = mk_label(panel, &eg_sora_20_bold, EG_FG_HAZE, "");
    lv_obj_set_pos(u->status, cx, 322);

    /* your strip */
    u->you_name = mk_label(panel, &eg_sora_16, EG_FG_HAZE, "");
    lv_obj_set_pos(u->you_name, cx, 358);
    u->you_clock = mk_label(panel, &eg_vt323_56, EG_PINK, "");
    lv_obj_align(u->you_clock, LV_ALIGN_TOP_RIGHT, -PAD, 340);

    /* buttons */
    lv_obj_t *row = lv_obj_create(panel);
    plain(row);
    lv_obj_set_size(row, cw, 56);
    lv_obj_set_pos(row, cx, 408);
    int bw = (cw - 16) / 3;
    u->btn_resign = mk_button(row, "Resign", EG_PINK, 0, bw);
    u->btn_flip = mk_button(row, "Flip", EG_CYAN, bw + 8, bw);
    u->btn_menu = mk_button(row, "Menu", EG_YELLOW, 2 * (bw + 8), bw);
    lv_obj_add_event_cb(u->btn_resign, resign_cb, LV_EVENT_CLICKED, scr);
    lv_obj_add_event_cb(u->btn_flip, flip_cb, LV_EVENT_CLICKED, scr);
    lv_obj_add_event_cb(u->btn_menu, menu_cb, LV_EVENT_CLICKED, scr);

    return scr;
}

void eg_game_set(lv_obj_t *screen, const eg_game_t *g)
{
    ui_t *u = U(screen);
    bool new_game = strcmp(u->g.id, g->id) != 0;
    u->g = *g;
    if (new_game) u->flipped = !g->you_white;
    u->selected = -1;
    close_overlay(&u->promo);
    redraw_board(u);
    redraw_panel(u);
}
