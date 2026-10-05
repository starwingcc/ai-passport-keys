#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t version;
    char ssid[33];
    char password[64];
} kp_wifi_config_t;

// An empty SSID disables automatic network time. Credentials are stored
// separately from shortcut/home records and never included in exports.
void kp_wifi_defaults(kp_wifi_config_t *wifi);
bool kp_wifi_valid(const kp_wifi_config_t *wifi);
