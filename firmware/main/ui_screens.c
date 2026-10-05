/* SPDX-License-Identifier: MIT */
#include "ui_screens.h"
#include "fonts.h"
#include "pieces.h"
#include "theme.h"
#include "pieces_store.h"
#include <string.h>
#include <stdio.h>

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

/* ---------- keyboard ----------
 * Phone-style: shift capitalizes one letter then drops back to lowercase, double-tap shift locks caps,
 * "123" and "#+=" pages cover every printable ASCII character a Wi-Fi password can use.
 * Replaces LVGL's default handler (which has a hide-keyboard key, cursor arrows and a "1#" key). */

#define K_SHIFT LV_SYMBOL_UP
#define K_BKSP  LV_SYMBOL_BACKSPACE
#define K_ENTER LV_SYMBOL_NEW_LINE

static const char *map_lower[] = {"q","w","e","r","t","y","u","i","o","p","\n",
    "a","s","d","f","g","h","j","k","l","\n",
    K_SHIFT,"z","x","c","v","b","n","m",K_BKSP,"\n",
    "123"," ",K_ENTER,""};
static const char *map_upper[] = {"Q","W","E","R","T","Y","U","I","O","P","\n",
    "A","S","D","F","G","H","J","K","L","\n",
    K_SHIFT,"Z","X","C","V","B","N","M",K_BKSP,"\n",
    "123"," ",K_ENTER,""};
static const char *map_num[] = {"1","2","3","4","5","6","7","8","9","0","\n",
    "-","/",":",";","(",")","$","&","@","\"","\n",
    "#+=",".",",","?","!","'",K_BKSP,"\n",
    "ABC"," ",K_ENTER,""};
static const char *map_sym[] = {"[","]","{","}","#","%","^","*","+","=","\n",
    "_","\\","|","~","<",">","`",".",",","\n",
    "123","?","!","'","\"","@",K_BKSP,"\n",
    "ABC"," ",K_ENTER,""};

#define NOREP LV_BUTTONMATRIX_CTRL_NO_REPEAT
/* widths are relative within a row and must stay 1..15 (higher bits are flags) */
static const lv_buttonmatrix_ctrl_t ctrl_alpha[] = {
    2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,
    3|NOREP,2,2,2,2,2,2,2,3,
    3|NOREP,10,3|NOREP};
static const lv_buttonmatrix_ctrl_t ctrl_num[] = {
    2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,
    3|NOREP,2,2,2,2,2,3,
    3|NOREP,10,3|NOREP};
static const lv_buttonmatrix_ctrl_t ctrl_sym[] = {
    2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,
    3|NOREP,2,2,2,2,2,3,
    3|NOREP,10,3|NOREP};

typedef struct {
    lv_obj_t *kb;
    bool caps_lock;
    uint32_t last_shift_ms;
    void (*on_enter)(void *ctx, lv_obj_t *ta);
    void *ctx;
} kbd_t;

static void kbd_mode(kbd_t *k, lv_keyboard_mode_t m)
{
    lv_keyboard_set_mode(k->kb, m);
    if (m == LV_KEYBOARD_MODE_TEXT_UPPER) {
        /* shift key (index 19) shows checked; locked caps also stays checked */
        lv_buttonmatrix_set_button_ctrl(k->kb, 19, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
}

static void kbd_event(lv_event_t *e)
{
    kbd_t *k = lv_event_get_user_data(e);
    lv_obj_t *kb = k->kb;
    uint32_t id = lv_buttonmatrix_get_selected_button(kb);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE) return;
    const char *t = lv_buttonmatrix_get_button_text(kb, id);
    if (!t) return;
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);
    lv_keyboard_mode_t mode = lv_keyboard_get_mode(kb);

    if (strcmp(t, K_SHIFT) == 0) {
        uint32_t now = lv_tick_get();
        if (mode == LV_KEYBOARD_MODE_TEXT_LOWER) {
            bool dbl = now - k->last_shift_ms < 400;
            k->caps_lock = dbl;
            kbd_mode(k, LV_KEYBOARD_MODE_TEXT_UPPER);
        } else if (!k->caps_lock && now - k->last_shift_ms < 400) {
            k->caps_lock = true;                       /* second tap of a double tap */
            kbd_mode(k, LV_KEYBOARD_MODE_TEXT_UPPER);
        } else {
            k->caps_lock = false;
            kbd_mode(k, LV_KEYBOARD_MODE_TEXT_LOWER);
        }
        k->last_shift_ms = now;
        return;
    }
    if (strcmp(t, "123") == 0) { kbd_mode(k, LV_KEYBOARD_MODE_SPECIAL); return; }
    if (strcmp(t, "#+=") == 0) { kbd_mode(k, LV_KEYBOARD_MODE_USER_1); return; }
    if (strcmp(t, "ABC") == 0) { k->caps_lock = false; kbd_mode(k, LV_KEYBOARD_MODE_TEXT_LOWER); return; }
    if (!ta) return;
    if (strcmp(t, K_BKSP) == 0) { lv_textarea_delete_char(ta); return; }
    if (strcmp(t, K_ENTER) == 0) { if (k->on_enter) k->on_enter(k->ctx, ta); return; }

    lv_textarea_add_text(ta, t);
    if (mode == LV_KEYBOARD_MODE_TEXT_UPPER && !k->caps_lock) kbd_mode(k, LV_KEYBOARD_MODE_TEXT_LOWER);
}

static kbd_t *kbd_create(lv_obj_t *parent, void (*on_enter)(void *, lv_obj_t *), void *ctx)
{
    kbd_t *k = lv_malloc(sizeof *k);
    memset(k, 0, sizeof *k);
    k->on_enter = on_enter;
    k->ctx = ctx;
    k->kb = lv_keyboard_create(parent);
    lv_obj_remove_event_cb(k->kb, lv_keyboard_def_event_cb);
    lv_keyboard_set_popovers(k->kb, false);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_TEXT_LOWER, map_lower, ctrl_alpha);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_TEXT_UPPER, map_upper, ctrl_alpha);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_SPECIAL, map_num, ctrl_num);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_USER_1, map_sym, ctrl_sym);
    lv_keyboard_set_mode(k->kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(k->kb, kbd_event, LV_EVENT_VALUE_CHANGED, k);

    lv_obj_set_size(k->kb, 800, 216);
    lv_obj_align(k->kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(k->kb, 6, 0);
    lv_obj_set_style_pad_gap(k->kb, 6, 0);
    lv_obj_set_style_bg_color(k->kb, EG_BG_VOID, 0);
    lv_obj_set_style_bg_color(k->kb, EG_SURFACE, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(k->kb, EG_CYAN, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(k->kb, EG_PINK, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(k->kb, EG_FG, LV_PART_ITEMS);
    lv_obj_set_style_text_color(k->kb, EG_BG_VOID, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(k->kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_border_width(k->kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(k->kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(k->kb, 8, LV_PART_ITEMS);
    return k;
}

/* ---------- Wi-Fi ---------- */

typedef struct {
    lv_obj_t *dd, *name, *pass, *err, *show_lbl;
    kbd_t *k;
    eg_wifi_cb_t cb;
    void (*rescan)(void);
    void (*back)(void);
    lv_obj_t *back_btn;
    bool have_list;          /* dropdown holds real networks (not "Searching...") */
} wifi_t;

#define OTHER_NET "Other network..."

/* The field you're typing in gets a thick pink border and the blinking cursor; the other goes quiet. */
static void set_target(wifi_t *w, lv_obj_t *ta)
{
    lv_obj_t *fields[] = {w->name, w->pass};
    for (int i = 0; i < 2; i++) {
        bool on = fields[i] == ta;
        lv_obj_set_style_border_color(fields[i], on ? EG_PINK : EG_FG_MIST, 0);
        lv_obj_set_style_border_width(fields[i], on ? 4 : 2, 0);
        lv_obj_set_style_bg_color(fields[i], on ? lv_color_hex(0x3A1A85) : EG_SURFACE, 0);
        if (on) lv_obj_add_state(fields[i], LV_STATE_FOCUSED);
        else lv_obj_remove_state(fields[i], LV_STATE_FOCUSED);
    }
    lv_keyboard_set_textarea(w->k->kb, ta);
}

static void field_click_cb(lv_event_t *e)
{
    set_target(lv_event_get_user_data(e), lv_event_get_target(e));
}

static bool other_selected(wifi_t *w)
{
    char buf[40];
    lv_dropdown_get_selected_str(w->dd, buf, sizeof buf);
    return strcmp(buf, OTHER_NET) == 0;
}

static void current_ssid(wifi_t *w, char out[33])
{
    out[0] = 0;
    if (other_selected(w)) {
        strncpy(out, lv_textarea_get_text(w->name), 32);
    } else if (w->have_list) {
        lv_dropdown_get_selected_str(w->dd, out, 33);
    }
    out[32] = 0;
}

static void do_connect(wifi_t *w)
{
    char ssid[33];
    current_ssid(w, ssid);
    if (!ssid[0]) { lv_label_set_text(w->err, other_selected(w) ? "Type the network name." : "Pick a network first."); return; }
    lv_label_set_text(w->err, "Connecting...");
    if (w->cb) w->cb(ssid, lv_textarea_get_text(w->pass));
}

static void enter_cb(void *ctx, lv_obj_t *ta)
{
    wifi_t *w = ctx;
    if (ta == w->name) set_target(w, w->pass);
    else do_connect(w);
}

static void show_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    bool hidden = lv_textarea_get_password_mode(w->pass);
    lv_textarea_set_password_mode(w->pass, !hidden);
    lv_label_set_text(w->show_lbl, hidden ? "Hide" : "Show");
}

static void connect_cb(lv_event_t *e) { do_connect(lv_event_get_user_data(e)); }

static void back_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    lv_label_set_text(w->err, "");
    if (w->back) w->back();
}

static void rescan_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    if (w->rescan) w->rescan();
}

static void dd_changed_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    lv_label_set_text(w->err, "");
    if (other_selected(w)) {
        lv_obj_remove_flag(w->name, LV_OBJ_FLAG_HIDDEN);
        set_target(w, w->name);
    } else {
        lv_obj_add_flag(w->name, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(w->pass, "");
        set_target(w, w->pass);
    }
}

static lv_obj_t *field(lv_obj_t *p, const char *placeholder, int x, int y, int wdt, bool pw, wifi_t *w)
{
    lv_obj_t *t = lv_textarea_create(p);
    lv_textarea_set_one_line(t, true);
    lv_textarea_set_placeholder_text(t, placeholder);
    lv_textarea_set_password_mode(t, pw);
    lv_obj_set_size(t, wdt, 52);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_style_bg_color(t, EG_SURFACE, 0);
    lv_obj_set_style_text_color(t, EG_FG, 0);
    lv_obj_set_style_text_font(t, &eg_sora_20, 0);
    lv_obj_set_style_border_color(t, EG_FG_MIST, 0);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_set_style_border_color(t, EG_PINK, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(t, EG_PINK, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_add_event_cb(t, field_click_cb, LV_EVENT_CLICKED, w);
    return t;
}

static lv_obj_t *pill(lv_obj_t *p, const char *text, lv_color_t bg, int x, int y, int wdt, int h)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_set_size(b, wdt, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = label(b, &eg_sora_20_bold, lv_color_white(), text);
    lv_obj_center(l);
    return b;
}

lv_obj_t *eg_wifi_create(lv_obj_t *parent, eg_wifi_cb_t on_connect, void (*on_rescan)(void), void (*on_back)(void))
{
    lv_obj_t *s = base(parent);
    wifi_t *w = lv_malloc(sizeof *w);
    memset(w, 0, sizeof *w);
    w->cb = on_connect;
    w->rescan = on_rescan;
    w->back = on_back;
    lv_obj_set_user_data(s, w);

    lv_obj_set_style_text_font(s, &eg_sora_20, 0);
    lv_obj_t *t = label(s, &eg_bungee_28, EG_CYAN, "CONNECT TO WI-FI");
    lv_obj_set_pos(t, 40, 20);

    /* network dropdown + rescan */
    w->dd = lv_dropdown_create(s);
    lv_obj_set_size(w->dd, 300, 52);
    lv_obj_set_pos(w->dd, 40, 72);
    lv_dropdown_set_options(w->dd, "Searching...");
    lv_obj_set_style_bg_color(w->dd, EG_SURFACE, 0);
    lv_obj_set_style_text_color(w->dd, EG_FG, 0);
    lv_obj_set_style_text_font(w->dd, &eg_sora_20, 0);
    lv_obj_set_style_text_font(w->dd, &lv_font_montserrat_20, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(w->dd, EG_CYAN, 0);
    lv_obj_set_style_border_width(w->dd, 2, 0);
    lv_obj_set_style_pad_ver(w->dd, 14, 0);
    lv_obj_t *list = lv_dropdown_get_list(w->dd);
    lv_obj_set_style_bg_color(list, EG_SURFACE, 0);
    lv_obj_set_style_text_color(list, EG_FG, 0);
    lv_obj_set_style_text_font(list, &eg_sora_20, 0);
    lv_obj_set_style_border_color(list, EG_CYAN, 0);
    lv_obj_set_style_border_width(list, 2, 0);
    lv_obj_set_style_text_line_space(list, 22, 0);
    lv_obj_set_style_max_height(list, 300, 0);
    lv_obj_set_style_bg_color(list, EG_CYAN, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(list, EG_BG_VOID, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_add_event_cb(w->dd, dd_changed_cb, LV_EVENT_VALUE_CHANGED, w);

    lv_obj_t *rb = pill(s, "", EG_SURFACE, 348, 72, 52, 52);
    lv_obj_t *rl = lv_obj_get_child(rb, 0);
    lv_obj_set_style_text_font(rl, &lv_font_montserrat_20, 0);
    lv_label_set_text(rl, LV_SYMBOL_REFRESH);
    lv_obj_add_event_cb(rb, rescan_cb, LV_EVENT_CLICKED, w);

    /* hidden-network name, only shown for "Other network..." */
    w->name = field(s, "Network name", 440, 72, 320, false, w);
    lv_obj_add_flag(w->name, LV_OBJ_FLAG_HIDDEN);

    /* password with Show / Hide */
    w->pass = field(s, "Password", 40, 134, 360, true, w);
    lv_obj_set_style_pad_right(w->pass, 86, 0);
    lv_obj_t *sb = lv_button_create(s);
    lv_obj_set_size(sb, 76, 40);
    lv_obj_set_pos(sb, 40 + 360 - 76 - 6, 134 + 6);
    lv_obj_set_style_bg_color(sb, EG_BG_STAGE, 0);
    lv_obj_set_style_shadow_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 6, 0);
    w->show_lbl = label(sb, &eg_sora_16, EG_CYAN, "Show");
    lv_obj_center(w->show_lbl);
    lv_obj_add_event_cb(sb, show_cb, LV_EVENT_CLICKED, w);

    lv_obj_add_event_cb(pill(s, "Connect", EG_PINK, 40, 198, 180, 48), connect_cb, LV_EVENT_CLICKED, w);
    w->err = label(s, &eg_sora_16, EG_MAGENTA, "");
    lv_label_set_long_mode(w->err, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(w->err, 520);
    lv_obj_set_pos(w->err, 236, 212);

    w->back_btn = pill(s, "Back", EG_SURFACE, 640, 16, 120, 44);
    lv_obj_add_event_cb(w->back_btn, back_cb, LV_EVENT_CLICKED, w);
    lv_obj_add_flag(w->back_btn, LV_OBJ_FLAG_HIDDEN);

    w->k = kbd_create(s, enter_cb, w);
    set_target(w, w->pass);
    return s;
}

void eg_wifi_set_back_visible(lv_obj_t *screen, bool visible)
{
    wifi_t *w = lv_obj_get_user_data(screen);
    if (visible) lv_obj_remove_flag(w->back_btn, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(w->back_btn, LV_OBJ_FLAG_HIDDEN);
}

static void (*s_on_wifi)(void);
void eg_screens_set_wifi_handler(void (*on_wifi)(void)) { s_on_wifi = on_wifi; }

static void wifi_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_on_wifi) s_on_wifi();
}

static void add_wifi_button(lv_obj_t *s)
{
    lv_obj_t *b = pill(s, "Change Wi-Fi", EG_SURFACE, 800 - 40 - 190, 480 - 30 - 44, 190, 44);
    lv_obj_add_event_cb(b, wifi_btn_cb, LV_EVENT_CLICKED, NULL);
}

void eg_wifi_set_error(lv_obj_t *screen, const char *msg)
{
    wifi_t *w = lv_obj_get_user_data(screen);
    lv_label_set_text(w->err, msg ? msg : "");
}

void eg_wifi_set_scanning(lv_obj_t *screen)
{
    wifi_t *w = lv_obj_get_user_data(screen);
    if (w->have_list) return;               /* keep the current list and selection while rescanning */
    lv_dropdown_set_options(w->dd, "Searching...");
}

void eg_wifi_set_networks(lv_obj_t *screen, const eg_ap_t *aps, int n)
{
    wifi_t *w = lv_obj_get_user_data(screen);
    char keep[33] = {0};
    if (w->have_list && !other_selected(w)) lv_dropdown_get_selected_str(w->dd, keep, sizeof keep);
    bool was_other = w->have_list && other_selected(w);

    lv_dropdown_clear_options(w->dd);
    int sel = 0;
    for (int i = 0; i < n; i++) {
        lv_dropdown_add_option(w->dd, aps[i].ssid, LV_DROPDOWN_POS_LAST);
        if (keep[0] && strcmp(keep, aps[i].ssid) == 0) sel = i;
    }
    lv_dropdown_add_option(w->dd, OTHER_NET, LV_DROPDOWN_POS_LAST);
    w->have_list = true;
    lv_dropdown_set_selected(w->dd, was_other || n == 0 ? n : sel);
    if (n == 0 && !was_other) {
        lv_label_set_text(w->err, "No networks found. Tap refresh or pick Other network.");
    }
    if (lv_dropdown_get_selected(w->dd) == (uint32_t)n) {
        lv_obj_remove_flag(w->name, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---------- pairing ---------- */

typedef struct { lv_obj_t *code, *qr, *status; } pair_t;

#define EG_PAIR_URL "https://endgam.es/board/pair"

lv_obj_t *eg_pair_create(lv_obj_t *parent)
{
    lv_obj_t *s = base(parent);
    pair_t *p = lv_malloc(sizeof *p);
    memset(p, 0, sizeof *p);
    lv_obj_set_user_data(s, p);

    /* left: QR on a white tile (phones need the light quiet zone) */
    lv_obj_t *tile = lv_obj_create(s);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tile, 300, 300);
    lv_obj_set_pos(tile, 60, 90);
    lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 16, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    p->qr = lv_qrcode_create(tile);
    lv_qrcode_set_size(p->qr, 260);
    lv_qrcode_set_dark_color(p->qr, EG_BG_VOID);
    lv_qrcode_set_light_color(p->qr, lv_color_white());
    lv_qrcode_update(p->qr, EG_PAIR_URL, strlen(EG_PAIR_URL));
    lv_obj_center(p->qr);

    /* right: instructions and the typed fallback */
    lv_obj_t *t = label(s, &eg_bungee_28, EG_YELLOW, "PAIR THIS CONSOLE");
    lv_obj_set_pos(t, 410, 90);
    lv_obj_t *a1 = label(s, &eg_sora_20, EG_FG, "Scan with your phone.");
    lv_obj_set_pos(a1, 410, 150);
    lv_obj_t *a2 = label(s, &eg_sora_16, EG_FG_HAZE, "No camera? Go to");
    lv_obj_set_pos(a2, 410, 206);
    lv_obj_t *u = label(s, &eg_sora_20_bold, EG_CYAN, "endgam.es/board/pair");
    lv_obj_set_pos(u, 410, 230);
    lv_obj_t *a3 = label(s, &eg_sora_16, EG_FG_HAZE, "and enter");
    lv_obj_set_pos(a3, 410, 262);
    p->code = label(s, &eg_bungee_44, EG_PINK, "------");
    lv_obj_set_style_text_letter_space(p->code, 8, 0);
    lv_obj_set_pos(p->code, 410, 288);

    p->status = label(s, &eg_sora_16, EG_FG_MIST, "");
    lv_obj_set_pos(p->status, 410, 360);
    add_wifi_button(s);
    return s;
}

void eg_pair_set_code(lv_obj_t *screen, const char *code, const char *claim_url)
{
    (void)claim_url;                     /* the QR always carries the code, whatever the server sends */
    pair_t *p = lv_obj_get_user_data(screen);
    lv_label_set_text(p->code, code);
    char url[96];
    snprintf(url, sizeof url, EG_PAIR_URL "?code=%s", code);
    lv_qrcode_update(p->qr, url, strlen(url));
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
    add_wifi_button(s);
    return s;
}

void eg_idle_refresh(lv_obj_t *s)
{
    static const char order[6] = {'k', 'q', 'r', 'b', 'n', 'p'};
    for (int32_t i = (int32_t)lv_obj_get_child_count(s) - 1; i >= 0; i--) {   /* drop the previous piece row */
        lv_obj_t *c = lv_obj_get_child(s, i);
        if (lv_obj_get_user_data(c) == (void *)1) lv_obj_delete(c);
    }
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
        lv_image_set_scale(im, 256 * 90 / EG_PIECE_PX);
        lv_obj_set_pos(im, i * 100 + 5, 5);
    }
}

/* ======================= home and sub-screens ======================= */

const char *eg_difficulty_label(const char *v)
{
    if (!v || !v[0]) return "";
    if (strcmp(v, "beginner") == 0) return "Easy";
    if (strcmp(v, "intermediate") == 0) return "Medium";
    if (strcmp(v, "advanced") == 0) return "Hard";
    if (strcmp(v, "expert") == 0) return "Expert";
    return v;
}

static lv_obj_t *back_button(lv_obj_t *s, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = pill(s, "Back", EG_SURFACE, 640, 16, 120, 44);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

static lv_obj_t *big_button(lv_obj_t *s, const char *text, lv_color_t bg, lv_color_t fg, int x, int y, int w, int h)
{
    lv_obj_t *b = lv_button_create(s);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_radius(b, 18, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = label(b, &eg_sora_20_bold, fg, text);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
    return b;
}

/* ---------- bottom tab bar (same tabs as the website: Play, Make, Sets, Rank, You) ---------- */

static void (*s_on_tab)(int);
void eg_screens_set_tab_handler(void (*on_tab)(int)) { s_on_tab = on_tab; }

static void tab_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_on_tab) s_on_tab(i);
}

#define NAV_Y 416
static void add_nav(lv_obj_t *s, int active)
{
    static const char *names[4] = {"Play", "Sets", "Rank", "You"};
    const lv_color_t accents[4] = {EG_CYAN, EG_YELLOW, EG_MINT, EG_PINK};
    lv_obj_t *bar = lv_obj_create(s);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bar, 800, 480 - NAV_Y);
    lv_obj_set_pos(bar, 0, NAV_Y);
    lv_obj_set_style_bg_color(bar, EG_BG_VOID, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bar, EG_SURFACE, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = lv_obj_create(bar);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(b, 200, 480 - NAV_Y - 2);
        lv_obj_set_pos(b, i * 200, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(b, i == active ? 4 : 0, 0);
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(b, accents[i], 0);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_center(label(b, &eg_sora_20_bold, i == active ? accents[i] : EG_FG_MIST, names[i]));
        if (i != active) lv_obj_add_event_cb(b, tab_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/* ---------- Play (home) ---------- */

typedef struct {
    eg_home_cb_t cb;
    lv_obj_t *list, *status;
    char ids[EG_HOME_MAX_GAMES][40];
    char inc_ids[EG_HOME_MAX_INCOMING][40];
} home_t;

static void home_play_cb(lv_event_t *e) { home_t *h = lv_event_get_user_data(e); if (h->cb.on_play_ai) h->cb.on_play_ai(); }
static void home_quick_cb(lv_event_t *e) { home_t *h = lv_event_get_user_data(e); if (h->cb.on_quick_match) h->cb.on_quick_match(); }
static void home_find_cb(lv_event_t *e) { home_t *h = lv_event_get_user_data(e); if (h->cb.on_find_player) h->cb.on_find_player(); }
static void home_open_cb(lv_event_t *e)
{
    home_t *h = lv_obj_get_user_data(lv_obj_get_screen(lv_event_get_target(e)));
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    if (h->cb.on_open_game) h->cb.on_open_game(h->ids[i]);
}

/* Accept / Decline. user_data: index * 2 + accept. Both buttons on the row go dead after one tap. */
static void home_respond_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    home_t *h = lv_obj_get_user_data(lv_obj_get_screen(btn));
    intptr_t v = (intptr_t)lv_event_get_user_data(e);
    int i = (int)(v / 2);
    bool accept = v % 2;
    lv_obj_t *row = lv_obj_get_parent(btn);
    for (uint32_t k = 0; k < lv_obj_get_child_count(row); k++) {
        lv_obj_t *c = lv_obj_get_child(row, k);
        if (lv_obj_check_type(c, &lv_button_class)) lv_obj_add_state(c, LV_STATE_DISABLED);
    }
    lv_label_set_text(h->status, accept ? "Starting the game..." : "");
    if (h->cb.on_respond) h->cb.on_respond(h->inc_ids[i], accept);
}

static lv_obj_t *small_button(lv_obj_t *p, const char *text, lv_color_t bg, lv_color_t fg, int w)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_set_size(b, w, 44);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_center(label(b, &eg_sora_16, fg, text));
    return b;
}

lv_obj_t *eg_home_create(lv_obj_t *parent, const eg_home_cb_t *cb)
{
    lv_obj_t *s = base(parent);
    home_t *h = lv_malloc(sizeof *h);
    memset(h, 0, sizeof *h);
    h->cb = *cb;
    lv_obj_set_user_data(s, h);

    /* Three ways to start a game, stacked on the left; your games on the right. */
    lv_obj_add_event_cb(big_button(s, "Play the computer", EG_PINK, lv_color_white(), 24, 24, 300, 114), home_play_cb, LV_EVENT_CLICKED, h);
    lv_obj_add_event_cb(big_button(s, "Quick match", EG_CYAN, EG_BG_VOID, 24, 152, 300, 114), home_quick_cb, LV_EVENT_CLICKED, h);
    lv_obj_add_event_cb(big_button(s, "Challenge a player", EG_YELLOW, EG_BG_VOID, 24, 280, 300, 114), home_find_cb, LV_EVENT_CLICKED, h);

    lv_obj_t *gt = label(s, &eg_sora_20_bold, EG_YELLOW, "Your games");
    lv_obj_set_pos(gt, 348, 24);
    h->list = lv_obj_create(s);
    lv_obj_set_size(h->list, 428, 338);
    lv_obj_set_pos(h->list, 348, 56);
    lv_obj_set_flex_flow(h->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(h->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(h->list, 0, 0);
    lv_obj_set_style_pad_all(h->list, 0, 0);
    lv_obj_set_style_pad_row(h->list, 8, 0);
    lv_obj_set_scroll_dir(h->list, LV_DIR_VER);

    h->status = label(s, &eg_sora_16, EG_FG_MIST, "Loading...");
    lv_obj_set_pos(h->status, 348, 64);
    add_nav(s, 0);
    return s;
}

void eg_home_set_status(lv_obj_t *screen, const char *msg)
{
    home_t *h = lv_obj_get_user_data(screen);
    lv_label_set_text(h->status, msg ? msg : "");
}

static lv_obj_t *list_row(lv_obj_t *list, bool highlight, lv_color_t edge)
{
    lv_obj_t *row = lv_button_create(list);
    lv_obj_set_size(row, LV_PCT(100), 70);
    lv_obj_set_style_pad_ver(row, 8, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_bg_color(row, EG_SURFACE, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_border_width(row, highlight ? 3 : 0, 0);
    lv_obj_set_style_border_color(row, edge, 0);
    return row;
}

void eg_home_set(lv_obj_t *screen, const eg_home_t *d)
{
    home_t *h = lv_obj_get_user_data(screen);
    lv_obj_clean(h->list);
    lv_label_set_text(h->status, d->n_games || d->n_incoming ? "" : "No games in progress.");

    /* Challenges waiting for your answer go first. */
    for (int i = 0; i < d->n_incoming; i++) {
        const eg_incoming_t *c = &d->incoming[i];
        snprintf(h->inc_ids[i], sizeof h->inc_ids[i], "%s", c->id);
        lv_obj_t *row = list_row(h->list, true, EG_CYAN);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);       /* only the two buttons react */
        lv_obj_t *n = label(row, &eg_sora_20_bold, EG_FG, c->name[0] ? c->name : "Player");
        lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
        lv_obj_set_width(n, 190);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_t *sub = label(row, &eg_sora_16, EG_CYAN, "");
        if (c->elo > 0) lv_label_set_text_fmt(sub, "Challenges you (%d)", c->elo);
        else lv_label_set_text(sub, "Challenges you");
        lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *no = small_button(row, "Decline", EG_BG_MIST, EG_FG, 92);
        lv_obj_align(no, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(no, home_respond_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(i * 2));
        lv_obj_t *yes = small_button(row, "Accept", EG_MINT, EG_BG_VOID, 92);
        lv_obj_align(yes, LV_ALIGN_RIGHT_MID, -100, 0);
        lv_obj_add_event_cb(yes, home_respond_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(i * 2 + 1));
    }

    for (int i = 0; i < d->n_games; i++) {
        const eg_home_game_t *g = &d->games[i];
        snprintf(h->ids[i], sizeof h->ids[i], "%s", g->id);
        lv_obj_t *row = list_row(h->list, g->your_turn, EG_PINK);
        lv_obj_t *n = label(row, &eg_sora_20_bold, EG_FG, "");
        lv_label_set_text_fmt(n, "vs %s", g->opponent[0] ? g->opponent : "Opponent");
        lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
        lv_obj_set_width(n, 240);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_t *sub = label(row, &eg_sora_16, EG_FG_HAZE, "");
        lv_label_set_text(sub, g->opponent_ai ? eg_difficulty_label(g->difficulty) : "");
        lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *turn = label(row, &eg_sora_16, g->your_turn ? EG_PINK : EG_FG_MIST, g->your_turn ? "Your turn" : "Their turn");
        lv_obj_align(turn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(row, home_open_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/* ---------- play the computer ---------- */

typedef struct {
    void (*start)(const char *, const char *);
    void (*back)(void);
    lv_obj_t *diff[4], *col[3], *status;
    int d, c;
} aisetup_t;

static const char *DIFF_API[4] = {"beginner", "intermediate", "advanced", "expert"};
static const char *DIFF_UI[4] = {"Easy", "Medium", "Hard", "Expert"};
static const char *COL_API[3] = {"white", "black", "random"};
static const char *COL_UI[3] = {"Me", "Computer", "Random"};   /* who moves first; maps to white / black / random */

static void seg_paint(lv_obj_t **btns, int n, int sel)
{
    for (int i = 0; i < n; i++) {
        lv_obj_set_style_bg_color(btns[i], i == sel ? EG_CYAN : EG_SURFACE, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btns[i], 0), i == sel ? EG_BG_VOID : EG_FG, 0);
    }
}

static void ai_diff_cb(lv_event_t *e)
{
    aisetup_t *a = lv_obj_get_user_data(lv_obj_get_screen(lv_event_get_target(e)));
    a->d = (int)(intptr_t)lv_event_get_user_data(e);
    seg_paint(a->diff, 4, a->d);
}
static void ai_col_cb(lv_event_t *e)
{
    aisetup_t *a = lv_obj_get_user_data(lv_obj_get_screen(lv_event_get_target(e)));
    a->c = (int)(intptr_t)lv_event_get_user_data(e);
    seg_paint(a->col, 3, a->c);
}
static void ai_start_cb(lv_event_t *e)
{
    aisetup_t *a = lv_event_get_user_data(e);
    lv_label_set_text(a->status, "Starting...");
    if (a->start) a->start(DIFF_API[a->d], COL_API[a->c]);
}
static void ai_back_cb(lv_event_t *e) { aisetup_t *a = lv_event_get_user_data(e); if (a->back) a->back(); }

static lv_obj_t *seg_button(lv_obj_t *s, const char *text, int x, int y, int w)
{
    lv_obj_t *b = lv_button_create(s);
    lv_obj_set_size(b, w, 64);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_center(label(b, &eg_sora_20_bold, EG_FG, text));
    return b;
}

lv_obj_t *eg_ai_setup_create(lv_obj_t *parent, void (*on_start)(const char *, const char *), void (*on_back)(void))
{
    lv_obj_t *s = base(parent);
    aisetup_t *a = lv_malloc(sizeof *a);
    memset(a, 0, sizeof *a);
    a->start = on_start;
    a->back = on_back;
    a->d = 1;                          /* Medium */
    a->c = 2;                          /* Random */
    lv_obj_set_user_data(s, a);

    lv_obj_set_pos(label(s, &eg_bungee_28, EG_PINK, "PLAY THE COMPUTER"), 40, 22);
    back_button(s, ai_back_cb, a);

    lv_obj_set_pos(label(s, &eg_sora_20_bold, EG_YELLOW, "Difficulty"), 40, 96);
    for (int i = 0; i < 4; i++) {
        a->diff[i] = seg_button(s, DIFF_UI[i], 40 + i * 182, 128, 170);
        lv_obj_add_event_cb(a->diff[i], ai_diff_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_set_pos(label(s, &eg_sora_20_bold, EG_YELLOW, "Who goes first"), 40, 222);
    for (int i = 0; i < 3; i++) {
        a->col[i] = seg_button(s, COL_UI[i], 40 + i * 242, 254, 230);
        lv_obj_add_event_cb(a->col[i], ai_col_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    seg_paint(a->diff, 4, a->d);
    seg_paint(a->col, 3, a->c);

    lv_obj_add_event_cb(pill(s, "Start", EG_PINK, 40, 366, 240, 64), ai_start_cb, LV_EVENT_CLICKED, a);
    a->status = label(s, &eg_sora_16, EG_MAGENTA, "");
    lv_obj_set_pos(a->status, 300, 388);
    return s;
}

void eg_ai_setup_set_status(lv_obj_t *screen, const char *msg)
{
    aisetup_t *a = lv_obj_get_user_data(screen);
    lv_label_set_text(a->status, msg ? msg : "");
}

/* ---------- challenge a friend ---------- */

typedef struct { void (*back)(void); void (*find)(void); lv_obj_t *tile, *qr, *status; } chal_t;

static void chal_back_cb(lv_event_t *e) { chal_t *c = lv_event_get_user_data(e); if (c->back) c->back(); }
static void chal_find_cb(lv_event_t *e) { chal_t *c = lv_event_get_user_data(e); if (c->find) c->find(); }

void eg_challenge_set_find(lv_obj_t *screen, void (*on_find)(void))
{
    chal_t *c = lv_obj_get_user_data(screen);
    c->find = on_find;
}

lv_obj_t *eg_challenge_create(lv_obj_t *parent, void (*on_back)(void))
{
    lv_obj_t *s = base(parent);
    chal_t *c = lv_malloc(sizeof *c);
    memset(c, 0, sizeof *c);
    c->back = on_back;
    lv_obj_set_user_data(s, c);

    c->tile = lv_obj_create(s);
    lv_obj_remove_flag(c->tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(c->tile, 300, 300);
    lv_obj_set_pos(c->tile, 60, 100);
    lv_obj_set_style_bg_color(c->tile, lv_color_white(), 0);
    lv_obj_set_style_border_width(c->tile, 0, 0);
    lv_obj_set_style_radius(c->tile, 16, 0);
    lv_obj_set_style_pad_all(c->tile, 0, 0);
    c->qr = lv_qrcode_create(c->tile);
    lv_qrcode_set_size(c->qr, 260);
    lv_qrcode_set_dark_color(c->qr, EG_BG_VOID);
    lv_qrcode_set_light_color(c->qr, lv_color_white());
    lv_obj_center(c->qr);
    lv_obj_add_flag(c->tile, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_pos(label(s, &eg_bungee_28, EG_CYAN, "CHALLENGE A FRIEND"), 40, 22);
    back_button(s, chal_back_cb, c);
    lv_obj_t *a1 = label(s, &eg_sora_20, EG_FG, "Have them scan this.");
    lv_obj_set_pos(a1, 400, 140);
    lv_obj_t *a2 = label(s, &eg_sora_16, EG_FG_HAZE, "They play against your piece set.\nThe game opens here when they join.");
    lv_obj_set_pos(a2, 400, 180);
    c->status = label(s, &eg_sora_16, EG_FG_MIST, "");
    lv_label_set_long_mode(c->status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(c->status, 360);
    lv_obj_set_pos(c->status, 400, 250);

    /* the QR only works for someone in the room */
    lv_obj_set_pos(label(s, &eg_sora_16, EG_FG_HAZE, "Not in the room with you?"), 400, 306);
    lv_obj_t *fb = pill(s, "Challenge a player", EG_YELLOW, 400, 334, 300, 56);
    lv_obj_set_style_text_color(lv_obj_get_child(fb, 0), EG_BG_VOID, 0);
    lv_obj_add_event_cb(fb, chal_find_cb, LV_EVENT_CLICKED, c);
    return s;
}

void eg_challenge_set(lv_obj_t *screen, const char *url, const char *status)
{
    chal_t *c = lv_obj_get_user_data(screen);
    if (url && url[0]) {
        lv_qrcode_update(c->qr, url, strlen(url));
        lv_obj_remove_flag(c->tile, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(c->tile, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(c->status, status ? status : "");
}

/* ---------- quick match ---------- */

typedef struct { void (*cancel)(void); void (*retry)(void); lv_obj_t *msg, *cancel_lbl, *retry_btn; } qm_t;

static void qm_cancel_cb(lv_event_t *e) { qm_t *q = lv_event_get_user_data(e); if (q->cancel) q->cancel(); }
static void qm_retry_cb(lv_event_t *e) { qm_t *q = lv_event_get_user_data(e); if (q->retry) q->retry(); }

lv_obj_t *eg_qm_create(lv_obj_t *parent, void (*on_cancel)(void), void (*on_retry)(void))
{
    lv_obj_t *s = base(parent);
    qm_t *q = lv_malloc(sizeof *q);
    memset(q, 0, sizeof *q);
    q->cancel = on_cancel;
    q->retry = on_retry;
    lv_obj_set_user_data(s, q);

    lv_obj_set_pos(label(s, &eg_bungee_28, EG_CYAN, "QUICK MATCH"), 40, 22);
    lv_obj_t *c = pill(s, "Cancel", EG_SURFACE, 640, 16, 120, 44);
    q->cancel_lbl = lv_obj_get_child(c, 0);
    lv_obj_add_event_cb(c, qm_cancel_cb, LV_EVENT_CLICKED, q);

    q->msg = label(s, &eg_sora_20, EG_FG, "Looking for an opponent...");
    lv_label_set_long_mode(q->msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(q->msg, 720);
    lv_obj_set_pos(q->msg, 40, 150);
    lv_obj_t *hint = label(s, &eg_sora_16, EG_FG_HAZE, "You get the first player who is also looking.\nYou play with your piece set.");
    lv_obj_set_pos(hint, 40, 230);

    q->retry_btn = pill(s, "Try again", EG_CYAN, 40, 330, 240, 64);
    lv_obj_set_style_text_color(lv_obj_get_child(q->retry_btn, 0), EG_BG_VOID, 0);
    lv_obj_add_event_cb(q->retry_btn, qm_retry_cb, LV_EVENT_CLICKED, q);
    lv_obj_add_flag(q->retry_btn, LV_OBJ_FLAG_HIDDEN);
    return s;
}

void eg_qm_set(lv_obj_t *screen, const char *msg, bool searching)
{
    qm_t *q = lv_obj_get_user_data(screen);
    lv_label_set_text(q->msg, msg ? msg : "");
    lv_label_set_text(q->cancel_lbl, searching ? "Cancel" : "Back");
    if (searching) lv_obj_add_flag(q->retry_btn, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(q->retry_btn, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- challenge a player ---------- */

#define FIND_LIST_Y 126
#define FIND_LIST_H_KB 130            /* list height while the keyboard is up (keyboard starts at y 264) */
#define FIND_LIST_H_FULL 330

typedef struct {
    eg_find_cb_t cb;
    lv_obj_t *root, *ta, *list, *msg;
    kbd_t *k;
    lv_obj_t *panel, *who, *first[3], *send, *status;   /* confirm panel */
    int n, pick, f;
    bool sent;
    eg_user_t users[EG_USERS_MAX];
} find_t;

static const char *FIRST_API[3] = {"me", "computer", "random"};   /* "computer" = the other player moves first */
static const char *FIRST_UI[3] = {"Me", "Them", "Random"};

static void find_keyboard(find_t *f, bool up)
{
    if (up) lv_obj_remove_flag(f->k->kb, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(f->k->kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(f->list, up ? FIND_LIST_H_KB : FIND_LIST_H_FULL);
}

static void find_text_cb(lv_event_t *e)
{
    find_t *f = lv_event_get_user_data(e);
    const char *q = lv_textarea_get_text(f->ta);
    if (strlen(q) < 2) eg_find_set_results(f->root, NULL, 0, "Type at least 2 letters of their name.");
    else lv_label_set_text(f->msg, "Searching...");
    if (f->cb.on_query) f->cb.on_query(q);
}
static void find_ta_click_cb(lv_event_t *e) { find_keyboard(lv_event_get_user_data(e), true); }
static void find_enter(void *ctx, lv_obj_t *ta) { (void)ta; find_keyboard(ctx, false); }
static void find_back_cb(lv_event_t *e) { find_t *f = lv_event_get_user_data(e); if (f->cb.on_back) f->cb.on_back(); }
static void find_link_cb(lv_event_t *e) { find_t *f = lv_event_get_user_data(e); if (f->cb.on_link) f->cb.on_link(); }

static void find_first_cb(lv_event_t *e)
{
    find_t *f = lv_obj_get_user_data(lv_event_get_target(e));   /* each button carries its find_t */
    f->f = (int)(intptr_t)lv_event_get_user_data(e);
    seg_paint(f->first, 3, f->f);
}

static void find_pick_cb(lv_event_t *e)
{
    find_t *f = lv_obj_get_user_data(lv_event_get_target(e));
    eg_find_confirm(f->root, (int)(intptr_t)lv_event_get_user_data(e));
}

static void find_send_cb(lv_event_t *e)
{
    find_t *f = lv_event_get_user_data(e);
    if (f->pick < 0 || f->pick >= f->n) return;
    lv_obj_add_flag(f->send, LV_OBJ_FLAG_HIDDEN);              /* one tap, one challenge */
    lv_label_set_text(f->status, "Sending...");
    if (f->cb.on_send) f->cb.on_send(f->users[f->pick].id, f->users[f->pick].name, FIRST_API[f->f]);
}

/* Panel Back: before sending, back to the search; after sending, home. */
static void find_panel_back_cb(lv_event_t *e)
{
    find_t *f = lv_event_get_user_data(e);
    if (f->sent) { if (f->cb.on_back) f->cb.on_back(); return; }
    lv_obj_add_flag(f->panel, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *eg_find_create(lv_obj_t *parent, const eg_find_cb_t *cb)
{
    lv_obj_t *s = base(parent);
    find_t *f = lv_malloc(sizeof *f);
    memset(f, 0, sizeof *f);
    f->cb = *cb;
    f->pick = -1;
    f->root = s;
    lv_obj_set_user_data(s, f);

    lv_obj_set_pos(label(s, &eg_bungee_28, EG_YELLOW, "CHALLENGE A PLAYER"), 40, 16);
    back_button(s, find_back_cb, f);

    f->ta = lv_textarea_create(s);
    lv_textarea_set_one_line(f->ta, true);
    lv_textarea_set_max_length(f->ta, 30);
    lv_textarea_set_placeholder_text(f->ta, "Their name");
    lv_obj_set_size(f->ta, 440, 52);
    lv_obj_set_pos(f->ta, 40, 66);
    lv_obj_set_style_bg_color(f->ta, lv_color_hex(0x3A1A85), 0);
    lv_obj_set_style_text_color(f->ta, EG_FG, 0);
    lv_obj_set_style_text_font(f->ta, &eg_sora_20, 0);
    lv_obj_set_style_border_color(f->ta, EG_PINK, 0);
    lv_obj_set_style_border_width(f->ta, 4, 0);
    lv_obj_set_style_bg_color(f->ta, EG_PINK, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_add_state(f->ta, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(f->ta, find_text_cb, LV_EVENT_VALUE_CHANGED, f);
    lv_obj_add_event_cb(f->ta, find_ta_click_cb, LV_EVENT_CLICKED, f);

    lv_obj_t *link = pill(s, "Share a link", EG_SURFACE, 500, 66, 260, 52);
    lv_obj_add_event_cb(link, find_link_cb, LV_EVENT_CLICKED, f);

    f->list = lv_obj_create(s);
    lv_obj_set_size(f->list, 720, FIND_LIST_H_KB);
    lv_obj_set_pos(f->list, 40, FIND_LIST_Y);
    lv_obj_set_flex_flow(f->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(f->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(f->list, 0, 0);
    lv_obj_set_style_pad_all(f->list, 0, 0);
    lv_obj_set_style_pad_row(f->list, 6, 0);
    lv_obj_set_scroll_dir(f->list, LV_DIR_VER);
    f->msg = label(s, &eg_sora_16, EG_FG_MIST, "");
    lv_obj_set_pos(f->msg, 40, FIND_LIST_Y + 8);

    f->k = kbd_create(s, find_enter, f);
    lv_keyboard_set_textarea(f->k->kb, f->ta);

    /* Confirm panel covers the screen. */
    f->panel = base(s);
    lv_obj_add_flag(f->panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(label(f->panel, &eg_bungee_28, EG_YELLOW, "CHALLENGE A PLAYER"), 40, 16);
    lv_obj_add_event_cb(pill(f->panel, "Back", EG_SURFACE, 640, 16, 120, 44), find_panel_back_cb, LV_EVENT_CLICKED, f);
    f->who = label(f->panel, &eg_sora_20_bold, EG_FG, "");
    lv_label_set_long_mode(f->who, LV_LABEL_LONG_DOT);
    lv_obj_set_width(f->who, 720);
    lv_obj_set_pos(f->who, 40, 90);
    lv_obj_set_pos(label(f->panel, &eg_sora_20_bold, EG_YELLOW, "Who goes first"), 40, 150);
    for (int i = 0; i < 3; i++) {
        f->first[i] = seg_button(f->panel, FIRST_UI[i], 40 + i * 242, 182, 230);
        lv_obj_set_user_data(f->first[i], f);
        lv_obj_add_event_cb(f->first[i], find_first_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    f->f = 0;
    seg_paint(f->first, 3, f->f);
    f->send = pill(f->panel, "Send challenge", EG_PINK, 40, 290, 300, 64);
    lv_obj_add_event_cb(f->send, find_send_cb, LV_EVENT_CLICKED, f);
    f->status = label(f->panel, &eg_sora_16, EG_FG_HAZE, "");
    lv_label_set_long_mode(f->status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(f->status, 720);
    lv_obj_set_pos(f->status, 40, 376);

    eg_find_reset(s);
    return s;
}

void eg_find_reset(lv_obj_t *screen)
{
    find_t *f = lv_obj_get_user_data(screen);
    f->sent = false;
    f->pick = -1;
    f->n = 0;
    lv_obj_add_flag(f->panel, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(f->ta, "");          /* fires VALUE_CHANGED: shows the 2-letter hint */
    lv_obj_clean(f->list);
    lv_label_set_text(f->msg, "Type at least 2 letters of their name.");
    find_keyboard(f, true);
}

void eg_find_set_results(lv_obj_t *screen, const eg_user_t *users, int n, const char *msg)
{
    find_t *f = lv_obj_get_user_data(screen);
    lv_obj_clean(f->list);
    f->n = 0;
    for (int i = 0; users && i < n && i < EG_USERS_MAX; i++) {
        f->users[i] = users[i];
        f->n++;
        lv_obj_t *row = lv_button_create(f->list);
        lv_obj_set_size(row, LV_PCT(100), 58);
        lv_obj_set_style_pad_hor(row, 14, 0);
        lv_obj_set_style_bg_color(row, EG_SURFACE, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_t *n_l = label(row, &eg_sora_20_bold, EG_FG, users[i].name[0] ? users[i].name : "Player");
        lv_label_set_long_mode(n_l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(n_l, 520);
        lv_obj_align(n_l, LV_ALIGN_LEFT_MID, 0, 0);
        if (users[i].elo > 0) {
            lv_obj_t *e = label(row, &eg_vt323_36, EG_CYAN, "");
            lv_label_set_text_fmt(e, "%d", users[i].elo);
            lv_obj_align(e, LV_ALIGN_RIGHT_MID, 0, 0);
        }
        lv_obj_set_user_data(row, f);
        lv_obj_add_event_cb(row, find_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_label_set_text(f->msg, f->n ? "" : (msg ? msg : ""));
}

void eg_find_confirm(lv_obj_t *screen, int index)
{
    find_t *f = lv_obj_get_user_data(screen);
    if (index < 0 || index >= f->n) return;
    f->pick = index;
    f->sent = false;
    lv_label_set_text_fmt(f->who, "Challenge %s?", f->users[index].name[0] ? f->users[index].name : "this player");
    lv_obj_remove_flag(f->send, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(f->status, "They get it in their games on endgam.es, the app, or their console.");
    lv_obj_remove_flag(f->panel, LV_OBJ_FLAG_HIDDEN);
}

void eg_find_set_status(lv_obj_t *screen, const char *msg, bool sent)
{
    find_t *f = lv_obj_get_user_data(screen);
    f->sent = sent;
    lv_label_set_text(f->status, msg ? msg : "");
    if (sent) lv_obj_add_flag(f->send, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(f->send, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- Sets tab (pick Default or one of yours; making a new one opens the QR screen) ---------- */

typedef struct { void (*pick)(const char *); void (*make)(void); lv_obj_t *list; char ids[EG_HOME_MAX_SETS][48]; } sets_t;

static void sets_pick_cb(lv_event_t *e)
{
    sets_t *t = lv_obj_get_user_data(lv_obj_get_screen(lv_event_get_target(e)));
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    uint32_t n = lv_obj_get_child_count(t->list);
    for (uint32_t k = 0; k < n; k++) {
        lv_obj_t *c = lv_obj_get_child(t->list, k);
        if (lv_obj_get_user_data(c) == t) lv_obj_set_style_border_width(c, c == lv_event_get_current_target(e) ? 4 : 0, 0);
    }
    if (t->pick) t->pick(t->ids[i]);
}

static void sets_make_cb(lv_event_t *e) { sets_t *t = lv_event_get_user_data(e); if (t->make) t->make(); }

static lv_obj_t *sets_heading(lv_obj_t *list, const char *text)
{
    lv_obj_t *l = label(list, &eg_sora_16, EG_FG_MIST, text);
    lv_obj_set_width(l, LV_PCT(100));
    return l;
}

lv_obj_t *eg_sets_create(lv_obj_t *parent, void (*on_pick)(const char *), void (*on_make)(void))
{
    lv_obj_t *s = base(parent);
    sets_t *t = lv_malloc(sizeof *t);
    memset(t, 0, sizeof *t);
    t->pick = on_pick;
    t->make = on_make;
    lv_obj_set_user_data(s, t);
    lv_obj_set_pos(label(s, &eg_sora_20_bold, EG_YELLOW, "Piece sets"), 24, 20);
    lv_obj_set_pos(label(s, &eg_sora_16, EG_FG_HAZE, "Tap one to play with it here and on the web."), 160, 23);
    lv_obj_t *mk = pill(s, "Make a new set", EG_YELLOW, 556, 12, 220, 44);
    lv_obj_set_style_text_color(lv_obj_get_child(mk, 0), EG_BG_VOID, 0);
    lv_obj_add_event_cb(mk, sets_make_cb, LV_EVENT_CLICKED, t);
    t->list = lv_obj_create(s);
    lv_obj_set_size(t->list, 752, 340);
    lv_obj_set_pos(t->list, 24, 68);
    lv_obj_set_flex_flow(t->list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_bg_opa(t->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(t->list, 0, 0);
    lv_obj_set_style_pad_all(t->list, 0, 0);
    lv_obj_set_style_pad_gap(t->list, 12, 0);
    add_nav(s, 1);
    return s;
}

static void set_tile(sets_t *t, const eg_set_t *st, const char *name, bool active, int i)
{
    snprintf(t->ids[i], sizeof t->ids[i], "%s", st->id);
    lv_obj_t *b = lv_button_create(t->list);
    lv_obj_set_user_data(b, t);
    lv_obj_set_size(b, 370, 104);
    lv_obj_set_style_bg_color(b, EG_SURFACE, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 8, 0);
    lv_obj_set_style_border_color(b, EG_PINK, 0);
    lv_obj_set_style_border_width(b, active ? 4 : 0, 0);
    /* preview tile: king and knight on a light square */
    lv_obj_t *tile = lv_obj_create(b);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(tile, 128, 84);
    lv_obj_align(tile, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(tile, EG_BOARD_LIGHT, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 10, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    const void *k = eg_thumb_find(st->preview_k), *n = eg_thumb_find(st->preview_n);
    if (k) { lv_obj_t *im = lv_image_create(tile); lv_image_set_src(im, k); lv_obj_align(im, LV_ALIGN_LEFT_MID, 6, 0); }
    if (n) { lv_obj_t *im = lv_image_create(tile); lv_image_set_src(im, n); lv_obj_align(im, LV_ALIGN_RIGHT_MID, -6, 0); }
    if (!k && !n) { lv_obj_t *w = label(tile, &eg_sora_16, EG_BG_VOID, "Loading..."); lv_obj_center(w); }
    lv_obj_t *l = label(b, &eg_sora_20_bold, EG_FG, name);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, 210);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 140, active ? -12 : 0);
    if (active) { lv_obj_t *a = label(b, &eg_sora_16, EG_PINK, "Playing with this"); lv_obj_align(a, LV_ALIGN_LEFT_MID, 140, 16); }
    lv_obj_add_event_cb(b, sets_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
}

void eg_sets_set(lv_obj_t *screen, const eg_home_t *h)
{
    sets_t *t = lv_obj_get_user_data(screen);
    lv_obj_clean(t->list);
    bool active_default = !h->active_set[0] || strcmp(h->active_set, "default") == 0;
    /* Default first, then the owner's own sets */
    int mine = 0;
    for (int i = 0; i < h->n_sets; i++)
        if (strcmp(h->sets[i].id, "default") == 0) set_tile(t, &h->sets[i], "Default", active_default, i);
    if (!h->n_sets) {                          /* home not loaded yet: still offer the default */
        static const eg_set_t def = {.id = "default"};
        set_tile(t, &def, "Default", active_default, 0);
    }
    sets_heading(t->list, "Your sets");
    for (int i = 0; i < h->n_sets; i++) {
        if (strcmp(h->sets[i].id, "default") == 0) continue;
        set_tile(t, &h->sets[i], h->sets[i].name[0] ? h->sets[i].name : "Untitled set", strcmp(h->sets[i].id, h->active_set) == 0, i);
        mine++;
    }
    if (!mine) {
        lv_obj_t *b = lv_button_create(t->list);
        lv_obj_set_size(b, 370, 104);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_border_color(b, EG_YELLOW, 0);
        lv_obj_set_style_radius(b, 14, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = label(b, &eg_sora_16, EG_YELLOW, "You haven't made one yet.\nTap to make a set from a photo.");
        lv_obj_center(l);
        lv_obj_add_event_cb(b, sets_make_cb, LV_EVENT_CLICKED, t);
    }
}

/* ---------- Make a set (opened from Sets; QR to the web maker) ---------- */

#define EG_MAKE_URL "https://endgam.es/make"

typedef struct { void (*back)(void); } make_t;
static void make_back_cb(lv_event_t *e) { make_t *m = lv_event_get_user_data(e); if (m->back) m->back(); }

lv_obj_t *eg_make_create(lv_obj_t *parent, void (*on_back)(void))
{
    lv_obj_t *s = base(parent);
    make_t *m = lv_malloc(sizeof *m);
    m->back = on_back;
    lv_obj_t *tile = lv_obj_create(s);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tile, 340, 340);
    lv_obj_set_pos(tile, 60, 70);
    lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 16, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_t *qr = lv_qrcode_create(tile);
    lv_qrcode_set_size(qr, 300);
    lv_qrcode_set_dark_color(qr, EG_BG_VOID);
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_update(qr, EG_MAKE_URL, strlen(EG_MAKE_URL));
    lv_obj_center(qr);
    lv_obj_set_pos(label(s, &eg_bungee_28, EG_YELLOW, "MAKE A SET"), 440, 110);
    lv_obj_set_pos(label(s, &eg_sora_20, EG_FG, "Scan to make one\non your phone."), 440, 166);
    lv_obj_t *t = label(s, &eg_sora_16, EG_FG_HAZE, "Pick a photo. Its colors and mood\nbecome a full piece set.\nIt shows up in Sets when it's ready.");
    lv_obj_set_pos(t, 440, 240);
    back_button(s, make_back_cb, m);
    return s;
}

/* ---------- Rank tab ---------- */

typedef struct { lv_obj_t *list, *you, *status; } rank_ui_t;

lv_obj_t *eg_rank_create(lv_obj_t *parent)
{
    lv_obj_t *s = base(parent);
    rank_ui_t *r = lv_malloc(sizeof *r);
    memset(r, 0, sizeof *r);
    lv_obj_set_user_data(s, r);
    lv_obj_set_pos(label(s, &eg_sora_20_bold, EG_MINT, "Leaderboard"), 24, 20);
    r->you = label(s, &eg_sora_16, EG_FG_HAZE, "");
    lv_obj_align(r->you, LV_ALIGN_TOP_RIGHT, -24, 23);
    r->list = lv_obj_create(s);
    lv_obj_set_size(r->list, 752, 352);
    lv_obj_set_pos(r->list, 24, 56);
    lv_obj_set_flex_flow(r->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(r->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r->list, 0, 0);
    lv_obj_set_style_pad_all(r->list, 0, 0);
    lv_obj_set_style_pad_row(r->list, 6, 0);
    r->status = label(s, &eg_sora_16, EG_FG_MIST, "Loading...");
    lv_obj_set_pos(r->status, 24, 64);
    add_nav(s, 2);
    return s;
}

void eg_rank_set(lv_obj_t *screen, const eg_rank_t *d, const char *error)
{
    rank_ui_t *r = lv_obj_get_user_data(screen);
    if (error) { lv_label_set_text(r->status, error); return; }
    lv_label_set_text(r->status, d->n ? "" : "No ranked players yet.");
    if (d->you_rank > 0) lv_label_set_text_fmt(r->you, "You: #%d  ·  %d", d->you_rank, d->you_elo);
    else lv_label_set_text(r->you, "You: unranked until your first finished game");
    lv_obj_clean(r->list);
    for (int i = 0; i < d->n; i++) {
        lv_obj_t *row = lv_obj_create(r->list);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row, LV_PCT(100), 48);
        lv_obj_set_style_bg_color(row, EG_SURFACE, 0);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_set_style_border_width(row, d->rows[i].you ? 3 : 0, 0);
        lv_obj_set_style_border_color(row, EG_MINT, 0);
        lv_obj_set_style_pad_hor(row, 14, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_t *n = label(row, &eg_sora_20_bold, d->rows[i].rank <= 3 ? EG_YELLOW : EG_FG_MIST, "");
        lv_label_set_text_fmt(n, "%d", d->rows[i].rank);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t *nm = label(row, &eg_sora_20, EG_FG, d->rows[i].name);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nm, 520);
        lv_obj_align(nm, LV_ALIGN_LEFT_MID, 56, 0);
        lv_obj_t *el = label(row, &eg_sora_20_bold, EG_CYAN, "");
        lv_label_set_text_fmt(el, "%d", d->rows[i].elo);
        lv_obj_align(el, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

/* ---------- You tab ---------- */

typedef struct { void (*forget)(void); lv_obj_t *name, *elo; uint32_t armed; } you_t;

static void you_forget_cb(lv_event_t *e)
{
    you_t *y = lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_current_target(e);
    uint32_t now = lv_tick_get();
    if (y->armed && now - y->armed < 3000) { y->armed = 0; if (y->forget) y->forget(); return; }
    y->armed = now;
    lv_label_set_text(lv_obj_get_child(btn, 0), "Tap again to unpair");
}

lv_obj_t *eg_you_create(lv_obj_t *parent, void (*on_forget)(void), const char *version)
{
    lv_obj_t *s = base(parent);
    you_t *y = lv_malloc(sizeof *y);
    memset(y, 0, sizeof *y);
    y->forget = on_forget;
    lv_obj_set_user_data(s, y);
    y->name = label(s, &eg_bungee_28, EG_PINK, "");
    lv_obj_set_pos(y->name, 40, 40);
    y->elo = label(s, &eg_sora_20, EG_FG_HAZE, "");
    lv_obj_set_pos(y->elo, 40, 86);

    lv_obj_t *wb = pill(s, "Change Wi-Fi", EG_SURFACE, 40, 150, 260, 60);
    lv_obj_add_event_cb(wb, wifi_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *fb = pill(s, "Forget this console", EG_SURFACE, 40, 226, 260, 60);
    lv_obj_set_style_text_color(lv_obj_get_child(fb, 0), EG_PINK_SOFT, 0);
    lv_obj_add_event_cb(fb, you_forget_cb, LV_EVENT_CLICKED, y);

    lv_obj_t *v = label(s, &eg_sora_16, EG_FG_MIST, "");
    lv_label_set_text_fmt(v, "Firmware %s (auto update)", version);
    lv_obj_set_pos(v, 40, 330);
    add_nav(s, 3);
    return s;
}

void eg_you_set(lv_obj_t *screen, const eg_home_t *h)
{
    you_t *y = lv_obj_get_user_data(screen);
    lv_label_set_text(y->name, h->name[0] ? h->name : "");
    if (h->elo >= 0) lv_label_set_text_fmt(y->elo, "Rating %d", h->elo);
    else lv_label_set_text(y->elo, "Unranked until your first finished game");
}
