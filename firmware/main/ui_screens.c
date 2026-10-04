/* SPDX-License-Identifier: MIT */
#include "ui_screens.h"
#include "fonts.h"
#include "pieces.h"
#include "theme.h"
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
    lv_obj_t *t = label(s, &eg_bungee_28, EG_YELLOW, "PAIR THIS BOARD");
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
        lv_image_set_scale(im, 256 * 90 / 60);
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

/* ---------- home ---------- */

typedef struct { eg_home_cb_t cb; lv_obj_t *who, *list, *status; char ids[EG_HOME_MAX_GAMES][40]; } home_t;

static void home_play_cb(lv_event_t *e) { home_t *h = lv_event_get_user_data(e); if (h->cb.on_play_ai) h->cb.on_play_ai(); }
static void home_chal_cb(lv_event_t *e) { home_t *h = lv_event_get_user_data(e); if (h->cb.on_challenge) h->cb.on_challenge(); }
static void home_sets_cb(lv_event_t *e) { home_t *h = lv_event_get_user_data(e); if (h->cb.on_sets) h->cb.on_sets(); }
static void home_open_cb(lv_event_t *e)
{
    home_t *h = lv_obj_get_user_data(lv_obj_get_screen(lv_event_get_target(e)));
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    if (h->cb.on_open_game) h->cb.on_open_game(h->ids[i]);
}

lv_obj_t *eg_home_create(lv_obj_t *parent, const eg_home_cb_t *cb)
{
    lv_obj_t *s = base(parent);
    home_t *h = lv_malloc(sizeof *h);
    memset(h, 0, sizeof *h);
    h->cb = *cb;
    lv_obj_set_user_data(s, h);

    lv_obj_t *t = label(s, &eg_bungee_28, EG_CYAN, "ENDGAMES");
    lv_obj_set_pos(t, 40, 22);
    h->who = label(s, &eg_sora_16, EG_FG_HAZE, "");
    lv_obj_set_pos(h->who, 40, 60);
    lv_obj_t *wb = pill(s, "Change Wi-Fi", EG_SURFACE, 800 - 40 - 170, 20, 170, 40);
    lv_obj_add_event_cb(wb, wifi_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(big_button(s, "Play the computer", EG_PINK, lv_color_white(), 40, 110, 300, 84), home_play_cb, LV_EVENT_CLICKED, h);
    lv_obj_add_event_cb(big_button(s, "Challenge a friend", EG_CYAN, EG_BG_VOID, 40, 208, 300, 84), home_chal_cb, LV_EVENT_CLICKED, h);
    lv_obj_add_event_cb(big_button(s, "Piece sets", EG_SURFACE, EG_FG, 40, 306, 300, 84), home_sets_cb, LV_EVENT_CLICKED, h);

    lv_obj_t *gt = label(s, &eg_sora_20_bold, EG_YELLOW, "Your games");
    lv_obj_set_pos(gt, 372, 110);
    h->list = lv_obj_create(s);
    lv_obj_set_size(h->list, 388, 290);
    lv_obj_set_pos(h->list, 372, 142);
    lv_obj_set_flex_flow(h->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(h->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(h->list, 0, 0);
    lv_obj_set_style_pad_all(h->list, 0, 0);
    lv_obj_set_style_pad_row(h->list, 8, 0);
    lv_obj_set_scroll_dir(h->list, LV_DIR_VER);

    h->status = label(s, &eg_sora_16, EG_FG_MIST, "Loading...");
    lv_obj_set_pos(h->status, 372, 150);
    return s;
}

void eg_home_set_status(lv_obj_t *screen, const char *msg)
{
    home_t *h = lv_obj_get_user_data(screen);
    lv_label_set_text(h->status, msg ? msg : "");
}

void eg_home_set(lv_obj_t *screen, const eg_home_t *d)
{
    home_t *h = lv_obj_get_user_data(screen);
    if (d->elo >= 0) lv_label_set_text_fmt(h->who, "%s  ·  %d", d->name, d->elo);
    else lv_label_set_text_fmt(h->who, "%s  ·  Unranked", d->name);
    lv_obj_clean(h->list);
    lv_label_set_text(h->status, d->n_games ? "" : "No games in progress.");
    for (int i = 0; i < d->n_games; i++) {
        const eg_home_game_t *g = &d->games[i];
        snprintf(h->ids[i], sizeof h->ids[i], "%s", g->id);
        lv_obj_t *row = lv_button_create(h->list);
        lv_obj_set_size(row, LV_PCT(100), 70);
        lv_obj_set_style_pad_ver(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 12, 0);
        lv_obj_set_style_bg_color(row, EG_SURFACE, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_border_width(row, g->your_turn ? 3 : 0, 0);
        lv_obj_set_style_border_color(row, EG_PINK, 0);
        lv_obj_t *n = label(row, &eg_sora_20_bold, EG_FG, "");
        lv_label_set_text_fmt(n, "vs %s", g->opponent[0] ? g->opponent : "Opponent");
        lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
        lv_obj_set_width(n, 240);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_t *sub = label(row, &eg_sora_16, EG_FG_HAZE, "");
        const char *col = g->your_color == 'b' ? "You're black" : "You're white";
        if (g->opponent_ai) lv_label_set_text_fmt(sub, "%s  ·  %s", eg_difficulty_label(g->difficulty), col);
        else lv_label_set_text(sub, col);
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
static const char *COL_UI[3] = {"White", "Black", "Random"};

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
    lv_obj_set_pos(label(s, &eg_sora_20_bold, EG_YELLOW, "You play"), 40, 222);
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

typedef struct { void (*back)(void); lv_obj_t *tile, *qr, *status; } chal_t;

static void chal_back_cb(lv_event_t *e) { chal_t *c = lv_event_get_user_data(e); if (c->back) c->back(); }

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

/* ---------- piece sets ---------- */

typedef struct { void (*pick)(const char *); void (*back)(void); lv_obj_t *list; char ids[EG_HOME_MAX_SETS][48]; } sets_t;

static void sets_back_cb(lv_event_t *e) { sets_t *t = lv_event_get_user_data(e); if (t->back) t->back(); }
static void sets_pick_cb(lv_event_t *e)
{
    sets_t *t = lv_obj_get_user_data(lv_obj_get_screen(lv_event_get_target(e)));
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    uint32_t n = lv_obj_get_child_count(t->list);
    for (uint32_t k = 0; k < n; k++) lv_obj_set_style_border_width(lv_obj_get_child(t->list, k), (intptr_t)k == i ? 3 : 0, 0);
    if (t->pick) t->pick(t->ids[i]);
}

lv_obj_t *eg_sets_create(lv_obj_t *parent, void (*on_pick)(const char *), void (*on_back)(void))
{
    lv_obj_t *s = base(parent);
    sets_t *t = lv_malloc(sizeof *t);
    memset(t, 0, sizeof *t);
    t->pick = on_pick;
    t->back = on_back;
    lv_obj_set_user_data(s, t);
    lv_obj_set_pos(label(s, &eg_bungee_28, EG_YELLOW, "PIECE SETS"), 40, 22);
    back_button(s, sets_back_cb, t);
    t->list = lv_obj_create(s);
    lv_obj_set_size(t->list, 720, 360);
    lv_obj_set_pos(t->list, 40, 92);
    lv_obj_set_flex_flow(t->list, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_bg_opa(t->list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(t->list, 0, 0);
    lv_obj_set_style_pad_all(t->list, 0, 0);
    lv_obj_set_style_pad_gap(t->list, 12, 0);
    return s;
}

void eg_sets_set(lv_obj_t *screen, const eg_home_t *h)
{
    sets_t *t = lv_obj_get_user_data(screen);
    lv_obj_clean(t->list);
    for (int i = 0; i < h->n_sets; i++) {
        snprintf(t->ids[i], sizeof t->ids[i], "%s", h->sets[i].id);
        lv_obj_t *b = lv_button_create(t->list);
        lv_obj_set_size(b, 354, 64);
        lv_obj_set_style_bg_color(b, EG_SURFACE, 0);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_border_color(b, EG_PINK, 0);
        lv_obj_set_style_border_width(b, strcmp(h->sets[i].id, h->active_set) == 0 ? 3 : 0, 0);
        lv_obj_t *l = label(b, &eg_sora_20_bold, EG_FG, h->sets[i].name[0] ? h->sets[i].name : "Untitled set");
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, 320);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 6, 0);
        lv_obj_add_event_cb(b, sets_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}
