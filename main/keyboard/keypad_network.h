#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef enum {
    KP_NET_DISABLED, KP_NET_CONNECTING, KP_NET_SYNCING,
    KP_NET_SYNCED, KP_NET_FAILED, KP_NET_PAUSED
} kp_network_status_t;

// Only the application worker owns Wi-Fi/SNTP lifecycle. Event callbacks just
// signal it; the HTTP task may read the atomic status. Suspend this service
// before the configuration hotspot takes ownership of the radio.
void kp_network_tick(uint32_t now_ms);
esp_err_t kp_network_suspend(void);
void kp_network_resume(void);
kp_network_status_t kp_network_status(void);
