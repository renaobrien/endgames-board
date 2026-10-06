/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>

/* Wi-Fi credentials and the device token live in NVS (flash), never in source. */
bool eg_store_load_wifi(char ssid[33], char pass[65]);
void eg_store_save_wifi(const char *ssid, const char *pass);
void eg_store_clear_wifi(void);
bool eg_store_load_token(char token[96]);
void eg_store_save_token(const char *token);
void eg_store_clear_token(void);
int eg_store_load_mode(void);              /* 0 dark, 1 light, 2 mono */
void eg_store_save_mode(int mode);
