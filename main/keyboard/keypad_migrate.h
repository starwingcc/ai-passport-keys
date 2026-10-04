#pragma once
#include "keypad_model.h"

// Exact v2 disk layout. Never read a v2 blob using the current config type.
typedef struct { uint16_t title, description, steps; uint8_t count, reserved; } kp_v2_binding_t;
typedef struct { uint16_t name; kp_v2_binding_t bindings[KP_BINDINGS]; } kp_v2_profile_t;
typedef struct {
    uint32_t version;
    uint16_t used;
    uint8_t profile_count, active_profile;
    kp_v2_profile_t profiles[KP_MAX_PROFILES];
    uint8_t data[KP_CONFIG_BYTES - 536];
} kp_v2_config_t;
_Static_assert(offsetof(kp_v2_config_t, data) == 536, "v2 disk header");
_Static_assert(sizeof(kp_v2_config_t) == KP_CONFIG_BYTES, "v2 disk budget");
bool kp_migrate_v2(const kp_v2_config_t *old, size_t size, kp_config_t *out);
