#pragma once

#include <stdbool.h>
#include <stdint.h>

// Continuous zero-online grace, separate from browser heartbeat expiration.
#define KP_TRANSFER_QUIET_MS UINT64_C(60000)

typedef enum {
    KP_TRANSFER_LISTEN,
    KP_TRANSFER_RUN,
    KP_TRANSFER_WAIT
} kp_transfer_phase_t;

// Published as one snapshot. A zero transition survives a short join/leave
// between two worker samples; generation separates catalogs from old sockets.
typedef struct {
    unsigned online;
    uint32_t generation;
    uint64_t zero_since_ms;
    bool allocated;
} kp_transfer_presence_t;

typedef struct {
    kp_transfer_phase_t phase;
    uint64_t zero_since_ms;
} kp_transfer_lifecycle_t;

void kp_transfer_lifecycle_enable(kp_transfer_lifecycle_t *s, uint64_t now_ms);
void kp_transfer_lifecycle_observe(kp_transfer_lifecycle_t *s, const kp_transfer_presence_t *presence);
bool kp_transfer_lifecycle_expired(const kp_transfer_lifecycle_t *s, uint64_t now_ms);
void kp_transfer_lifecycle_reset(kp_transfer_lifecycle_t *s);
