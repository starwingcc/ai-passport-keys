#pragma once
#include "keypad_transfer.h"
#include "esp_err.h"
typedef enum { KP_AP_OFF, KP_AP_STARTING, KP_AP_ACTIVE, KP_AP_STOPPING, KP_AP_FAILED } kp_ap_state_t;
typedef struct {
    kp_transfer_view_t transfer;
    kp_ap_state_t ap;
    bool forget_requested;
    char ssid[32], password[16];
} kp_network_view_t;
// This task exclusively owns STA, SNTP, AP, HTTP and their teardown. Keyboard
// worker requests transitions without waiting or holding the LVGL lock.
esp_err_t kp_network_service_start(void);
void kp_network_service_transfer(bool enable);
void kp_network_service_ap(bool enable);
void kp_network_service_get(kp_network_view_t *view);
