#pragma once
#include "keypad_model.h"
#include "keypad_home.h"
#include "keypad_wifi.h"
#include <string.h>

// One versioned record for every setting edited by the web workbench.
// Keep the shortcut arena last so unused arena bytes need not be persisted.
typedef struct {
    uint32_t version;
    kp_home_config_t home;
    kp_wifi_config_t wifi;
    kp_config_t config;
} kp_settings_t;

static inline void kp_settings_defaults(kp_settings_t *s) {
    memset(s, 0, sizeof(*s));
    s->version = 1;
    kp_home_defaults(&s->home);
    kp_wifi_defaults(&s->wifi);
    kp_defaults(&s->config);
}
static inline bool kp_settings_valid(const kp_settings_t *s, bool stored) {
    return s && s->version == 1 && kp_home_valid(&s->home, stored) &&
           kp_wifi_valid(&s->wifi) && kp_config_valid(&s->config);
}
static inline size_t kp_settings_size(const kp_settings_t *s) {
    return offsetof(kp_settings_t, config) + kp_config_size(&s->config);
}
