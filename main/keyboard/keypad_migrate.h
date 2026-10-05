#pragma once
#include "keypad_model.h"

// Exact v2 disk layout. Never read a v2 blob using the current config type.
typedef struct { uint16_t title, description, steps; uint8_t count, reserved; } kp_v2_binding_t;
typedef struct { uint16_t name; kp_v2_binding_t bindings[8]; } kp_v2_profile_t;
typedef struct {
    uint32_t version;
    uint16_t used;
    uint8_t profile_count, active_profile;
    kp_v2_profile_t profiles[8];
    uint8_t data[KP_CONFIG_BYTES - 536];
} kp_v2_config_t;
_Static_assert(offsetof(kp_v2_config_t, data) == 536, "v2 disk header");
_Static_assert(sizeof(kp_v2_config_t) == KP_CONFIG_BYTES, "v2 disk budget");
bool kp_migrate_v2(const kp_v2_config_t *old, size_t size, kp_config_t *out);

// Exact v3 disk layout, including eight slots and byte-addressed step offsets.
typedef struct { uint16_t title, steps; uint8_t count, reserved; } kp_v3_binding_t;
typedef struct { uint16_t name; kp_v3_binding_t bindings[8]; } kp_v3_profile_t;
typedef struct {
    uint32_t version;
    uint16_t used;
    uint8_t profile_count, active_profile;
    kp_v3_profile_t profiles[8];
    uint8_t data[KP_CONFIG_BYTES - 408];
} kp_v3_config_t;
_Static_assert(offsetof(kp_v3_config_t, data) == 408, "v3 disk header");
_Static_assert(sizeof(kp_v3_config_t) == KP_CONFIG_BYTES, "v3 disk budget");
_Static_assert(sizeof(kp_legacy_config_t) == 84, "v1 disk budget");
_Static_assert(sizeof(((kp_config_t *)0)->data) >= sizeof(((kp_v3_config_t *)0)->data), "v3 migration capacity");
_Static_assert(sizeof(((kp_config_t *)0)->data) >= sizeof(((kp_v2_config_t *)0)->data), "v2 migration capacity");
bool kp_migrate_v3(const kp_v3_config_t *old, size_t size, kp_config_t *out);
// v4 has the same header and 12-byte steps, but keyboard payloads must be zero.
bool kp_migrate_v4(kp_config_t *config, size_t size);
