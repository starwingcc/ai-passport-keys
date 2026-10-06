#pragma once
#include <stdbool.h>
#include <stdint.h>
// Runtime-only state: never stored in the keyboard configuration or NVS.
typedef enum { KP_TRANSFER_OFF, KP_TRANSFER_NO_WIFI, KP_TRANSFER_CONNECTING,
               KP_TRANSFER_READY, KP_TRANSFER_RECONNECTING, KP_TRANSFER_FAILED,
               KP_TRANSFER_STOPPING } kp_transfer_state_t;
typedef struct {
    bool enabled;
    kp_transfer_state_t state;
    unsigned peers;
    char ip[16];
} kp_transfer_view_t;
#define KP_SETTINGS_COUNT 5U
static inline unsigned kp_settings_first(unsigned selected) { return selected >= 4 ? selected - 3 : 0; }
