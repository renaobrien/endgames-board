/* SPDX-License-Identifier: MIT */
#include "ui_screens.h"
#include "fonts.h"
#include "pieces.h"
#include "theme.h"
#include <string.h>

static lv_obj_t *base(lv_obj_t *parent)
{
    lv_obj_t *s = lv_obj_create(parent);
    lv_obj_set_size(s, 800, 480);
    lv_obj_set_style_bg_color(s, EG_BG_STAGE, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_radius(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

static lv_obj_t *label(lv_obj_t *p, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, t);
    return l;
}

/* ---------- Wi-Fi ---------- */

typedef struct { lv_obj_t *ssid, *pass, *kb, *err; eg_wifi_cb_t cb; } wifi_t;

static void focus_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    lv_keyboard_set_textarea(w->kb, lv_event_get_target(e));
}

static void connect_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    if (w->cb) w->cb(lv_textarea_get_text(w->ssid), lv_textarea_get_text(w->pass));
}

static lv_obj_t *field(lv_obj_t *p, const char *placeholder, int y, bool pw, wifi_t *w)
{
    lv_obj_t *t = lv_textarea_create(p);
    lv_textarea_set_one_line(t, true);
    lv_textarea_set_placeholder_text(t, placeholder);
    lv_textarea_set_password_mode(t, pw);
    lv_obj_set_size(t, 360, 52);
    lv_obj_set_pos(t, 40, y);
    lv_obj_set_style_bg_color(t, EG_SURFACE, 0);
    lv_obj_set_style_text_color(t, EG_FG, 0);
    lv_obj_set_style_text_font(t, &eg_sora_20, 0);
    lv_obj_set_style_border_color(t, EG_CYAN, 0);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_add_event_cb(t, focus_cb, LV_EVENT_FOCUSED, w);
    return t;
}

lv_obj_t *eg_wifi_create(lv_obj_t *parent, eg_wifi_cb_t on_connect)
{
    lv_obj_t *s = base(parent);
    wifi_t *w = lv_malloc(sizeof *w);
    memset(w, 0, sizeof *w);
    w->cb = on_connect;
    lv_obj_set_user_data(s, w);

    lv_obj_set_style_text_font(s, &eg_sora_20, 0);
    lv_obj_t *t = label(s, &eg_bungee_28, EG_CYAN, "CONNECT TO WI-FI");
    lv_obj_set_pos(t, 40, 24);
    w->ssid = field(s, "Network name", 72, false, w);
    w->pass = field(s, "Password", 132, true, w);

    lv_obj_t *b = lv_button_create(s);
    lv_obj_set_size(b, 200, 52);
    lv_obj_set_pos(b, 40, 198);
    lv_obj_set_style_bg_color(b, EG_PINK, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(b, connect_cb, LV_EVENT_CLICKED, w);
    lv_obj_center(label(b, &eg_sora_20_bold, lv_color_white(), "Connect"));

    w->err = label(s, &eg_sora_16, EG_MAGENTA, "");
    lv_obj_set_pos(w->err, 260, 212);

    lv_obj_t *hint = label(s, &eg_sora_16, EG_FG_HAZE, "Wi-Fi 2.4 or 5 GHz. Stored on this board only.");
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, 320);
    lv_obj_set_pos(hint, 440, 82);

    w->kb = lv_keyboard_create(s);
    lv_obj_set_size(w->kb, 800, 200);
    lv_obj_set_style_bg_color(w->kb, EG_BG_VOID, 0);
    lv_obj_set_style_bg_color(w->kb, EG_SURFACE, LV_PART_ITEMS);
    lv_obj_set_style_text_color(w->kb, EG_FG, LV_PART_ITEMS);
    lv_obj_set_style_border_width(w->kb, 0, LV_PART_ITEMS);
    lv_obj_align(w->kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(w->kb, w->ssid);
    return s;
}

void eg_wifi_set_error(lv_obj_t *screen, const char *msg)
{
    wifi_t *w = lv_obj_get_user_data(screen);
    lv_label_set_text(w->err, msg ? msg : "");
}

/* ---------- pairing ---------- */

typedef struct { lv_obj_t *code, *url, *status; } pair_t;

lv_obj_t *eg_pair_create(lv_obj_t *parent)
{
    lv_obj_t *s = base(parent);
    pair_t *p = lv_malloc(sizeof *p);
    memset(p, 0, sizeof *p);
    lv_obj_set_user_data(s, p);

    lv_obj_t *t = label(s, &eg_bungee_28, EG_YELLOW, "PAIR THIS BOARD");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_t *step = label(s, &eg_sora_20, EG_FG_HAZE, "Go to");
    lv_obj_align(step, LV_ALIGN_TOP_MID, 0, 110);
    p->url = label(s, &eg_sora_20_bold, EG_CYAN, "endgam.es/board/pair");
    lv_obj_align(p->url, LV_ALIGN_TOP_MID, 0, 142);
    lv_obj_t *step2 = label(s, &eg_sora_20, EG_FG_HAZE, "and type this code");
    lv_obj_align(step2, LV_ALIGN_TOP_MID, 0, 178);

    p->code = label(s, &eg_bungee_44, EG_PINK, "------");
    lv_obj_set_style_text_letter_space(p->code, 12, 0);
    lv_obj_align(p->code, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_font(p->code, &eg_vt323_56, 0);   /* big and readable across a room */
    lv_obj_set_style_text_font(p->code, &eg_bungee_44, 0);

    p->status = label(s, &eg_sora_16, EG_FG_MIST, "Waiting for you...");
    lv_obj_align(p->status, LV_ALIGN_BOTTOM_MID, 0, -40);
    return s;
}

void eg_pair_set_code(lv_obj_t *screen, const char *code, const char *claim_url)
{
    pair_t *p = lv_obj_get_user_data(screen);
    lv_label_set_text(p->code, code);
    if (claim_url) lv_label_set_text(p->url, claim_url);
}

void eg_pair_set_status(lv_obj_t *screen, const char *msg)
{
    pair_t *p = lv_obj_get_user_data(screen);
    lv_label_set_text(p->status, msg);
}

/* ---------- idle ---------- */

lv_obj_t *eg_idle_create(lv_obj_t *parent)
{
    lv_obj_t *s = base(parent);
    lv_obj_t *t = label(s, &eg_bungee_28, EG_CYAN, "NO GAME IN PROGRESS");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_t *sub = label(s, &eg_sora_20, EG_FG_HAZE, "Start one on endgam.es and it shows up here.");
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 92);
    eg_idle_refresh(s);
    return s;
}

void eg_idle_refresh(lv_obj_t *s)
{
    static const char order[6] = {'k', 'q', 'r', 'b', 'n', 'p'};
    lv_obj_t *old = lv_obj_get_child(s, -1);
    if (old && lv_obj_get_user_data(old) == (void *)1) lv_obj_delete(old);
    lv_obj_t *row = lv_obj_create(s);
    lv_obj_set_user_data(row, (void *)1);
    lv_obj_set_size(row, 6 * 100, 100);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 70);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 6; i++) {
        const void *src = eg_piece_src('w', order[i]);
        if (!src) continue;
        lv_obj_t *im = lv_image_create(row);
        lv_image_set_src(im, src);
        lv_image_set_scale(im, 256 * 90 / 60);
        lv_obj_set_pos(im, i * 100 + 5, 5);
    }
}
