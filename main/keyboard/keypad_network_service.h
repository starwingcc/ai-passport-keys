#pragma once
#include "keypad_transfer.h"
#include "esp_err.h"
#include "keypad_lan.h"
typedef struct {
    kp_transfer_view_t transfer;
    kp_lan_view_t lan;
    bool forget_requested;
    char ssid[32], password[16];
} kp_network_view_t;
// This task exclusively owns STA, SNTP, AP, HTTP and their teardown. Keyboard
// worker requests transitions without waiting or holding the LVGL lock.
esp_err_t kp_network_service_start(void);
void kp_network_service_transfer(bool enable);
void kp_network_service_ap(bool enable);
void kp_network_service_config(bool enable, bool wifi);
void kp_network_service_get(kp_network_view_t *view);
