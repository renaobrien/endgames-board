/* SPDX-License-Identifier: MIT */
#include "ota.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "eg_ota";

const char *eg_ota_version(void) { return esp_app_get_description()->version; }

void eg_ota_mark_good(void)
{
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "version %s confirmed", eg_ota_version());
    }
}

/* "1.10.0" > "1.9.3" */
static bool newer(const char *a, const char *b)
{
    int x[3] = {0}, y[3] = {0};
    sscanf(a, "%d.%d.%d", &x[0], &x[1], &x[2]);
    sscanf(b, "%d.%d.%d", &y[0], &y[1], &y[2]);
    for (int i = 0; i < 3; i++) if (x[i] != y[i]) return x[i] > y[i];
    return false;
}

static bool fetch_manifest(char version[32], char url[256])
{
    esp_http_client_config_t cfg = {.url = EG_OTA_MANIFEST, .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 15000};
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return false;
    bool ok = false;
    char *buf = NULL;
    if (esp_http_client_open(c, 0) == ESP_OK) {
        int len = esp_http_client_fetch_headers(c);
        if (esp_http_client_get_status_code(c) == 200 && len >= 0 && len < 2048) {
            int cap = len > 0 ? len : 2047;
            buf = calloc(1, cap + 1);
            int n = buf ? esp_http_client_read_response(c, buf, cap) : -1;
            if (n > 0) {
                cJSON *j = cJSON_Parse(buf);
                const cJSON *v = cJSON_GetObjectItem(j, "version"), *u = cJSON_GetObjectItem(j, "url");
                if (cJSON_IsString(v) && cJSON_IsString(u) && strncmp(u->valuestring, "https://", 8) == 0) {
                    strncpy(version, v->valuestring, 31); version[31] = 0;
                    strncpy(url, u->valuestring, 255); url[255] = 0;
                    ok = true;
                }
                cJSON_Delete(j);
            }
        }
    }
    free(buf);
    esp_http_client_cleanup(c);
    return ok;
}

bool eg_ota_check_and_update(void (*on_start)(const char *))
{
    char version[32], url[256];
    if (!fetch_manifest(version, url)) { ESP_LOGW(TAG, "no manifest"); return false; }
    if (!newer(version, eg_ota_version())) { ESP_LOGI(TAG, "up to date (%s)", eg_ota_version()); return false; }
    ESP_LOGI(TAG, "updating %s -> %s", eg_ota_version(), version);
    if (on_start) on_start(version);

    esp_http_client_config_t http = {.url = url, .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 30000, .keep_alive_enable = true};
    esp_https_ota_config_t ota = {.http_config = &http};
    esp_err_t e = esp_https_ota(&ota);
    if (e != ESP_OK) { ESP_LOGE(TAG, "update failed: %s", esp_err_to_name(e)); return false; }
    ESP_LOGI(TAG, "update done, restarting");
    esp_restart();
    return true;
}
