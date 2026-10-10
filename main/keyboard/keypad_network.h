#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KP_NET_DISABLED, KP_NET_CONNECTING, KP_NET_SYNCING,
    KP_NET_SYNCED, KP_NET_FAILED, KP_NET_PAUSED
} kp_network_status_t;

// Only the network worker owns Wi-Fi/SNTP lifecycle. Event callbacks just
// signal it; the HTTP task may read the atomic status. Suspend this service
// before the configuration hotspot takes ownership of the radio.
void kp_network_tick(uint32_t now_ms);
esp_err_t kp_network_suspend(void);
void kp_network_resume(void);
kp_network_status_t kp_network_status(void);

// Worker-only radio lease. Time synchronization shares it without stopping STA.
void kp_network_keep_online(bool keep);
bool kp_network_ip(char out[16]);
// Worker-only: lend the running station to configuration scans/tests without
// destroying its netif. Event callbacks keep tracking DHCP during the loan.
void kp_network_setup_pause(bool pause, uint32_t now_ms);
// Worker-only, while setup owns the station: DHCP for a candidate test, then
// restore the fixed LAN address before reconnecting the original association.
esp_err_t kp_network_setup_test_address(bool candidate);
