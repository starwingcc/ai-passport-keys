#pragma once
#include <stdbool.h>
typedef enum { KP_AP_OFF, KP_AP_STARTING, KP_AP_ACTIVE, KP_AP_STOPPING,
               KP_AP_FAILED, KP_AP_NO_WIFI } kp_ap_state_t;
// Runtime configuration view; no credentials or persistent settings.
typedef struct {
    kp_ap_state_t state;
    bool wifi, configured;
    unsigned online;
    char ssid[33], ip[16];
} kp_lan_view_t;
