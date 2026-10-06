#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t version;
    char ssid[33];
    char password[64];
} kp_wifi_config_t;

// An empty SSID disables automatic network time. Complete configuration
// exports include these credentials; scenario-only exports omit them.
void kp_wifi_defaults(kp_wifi_config_t *wifi);
bool kp_wifi_valid(const kp_wifi_config_t *wifi);
