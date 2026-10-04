/* SPDX-License-Identifier: MIT */
#include "store.h"
#include "nvs.h"
#include <string.h>

#define NS "endgames"

static bool get(const char *key, char *out, size_t n)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = n;
    bool ok = nvs_get_str(h, key, out, &len) == ESP_OK && out[0];
    nvs_close(h);
    return ok;
}

static void put(const char *key, const char *val)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (val) nvs_set_str(h, key, val); else nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
}

bool eg_store_load_wifi(char ssid[33], char pass[65]) { return get("ssid", ssid, 33) && (get("pass", pass, 65) || (pass[0] = 0, true)); }
void eg_store_save_wifi(const char *ssid, const char *pass) { put("ssid", ssid); put("pass", pass); }
void eg_store_clear_wifi(void) { put("ssid", NULL); put("pass", NULL); }
bool eg_store_load_token(char token[96]) { return get("token", token, 96); }
void eg_store_save_token(const char *token) { put("token", token); }
void eg_store_clear_token(void) { put("token", NULL); }
