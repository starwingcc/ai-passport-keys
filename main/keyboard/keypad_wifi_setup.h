#pragma once
#include "keypad_wifi.h"
#include <stddef.h>

#define KP_WIFI_SCAN_RESULTS 16U
typedef enum { KP_WIFI_SCAN = 1, KP_WIFI_TEST = 2 } kp_wifi_job_kind_t;
typedef enum {
    KP_WIFI_IDLE, KP_WIFI_QUEUED, KP_WIFI_SCANNING, KP_WIFI_CONNECTING,
    // Preserve web API state values after removing the time-sync states.
    KP_WIFI_SCAN_DONE = 5, KP_WIFI_VERIFIED, KP_WIFI_AUTH_FAILED,
    KP_WIFI_NOT_FOUND, KP_WIFI_TIMEOUT, KP_WIFI_ERROR = 11,
    KP_WIFI_UNSUPPORTED, KP_WIFI_RESTORING
} kp_wifi_job_state_t;
typedef struct { char ssid[33]; int8_t rssi; bool secured; } kp_wifi_scan_result_t;
typedef struct {
    uint32_t id;
    kp_wifi_job_kind_t kind;
    kp_wifi_job_state_t state;
    bool busy, connected, restore_failed;
    uint16_t reason;
    char ssid[33];
    unsigned count;
    kp_wifi_scan_result_t networks[KP_WIFI_SCAN_RESULTS];
} kp_wifi_job_t;

// Pure helpers: bounded strongest-SSID list, timeout policy and IEEE/IDF reason
// classification. Authentication failure is not proof of an incorrect password.
void kp_wifi_scan_add(kp_wifi_job_t *job, const char *ssid, int rssi, bool secured);
kp_wifi_job_state_t kp_wifi_failure(uint16_t reason);
bool kp_wifi_job_expired(kp_wifi_job_state_t state, uint32_t elapsed_ms);
