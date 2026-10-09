#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    KP_PAIRING_NONE,
    KP_PAIRING_KEY_MISSING,
    KP_PAIRING_RETRY_FAILED,
} kp_pairing_hint_t;

typedef struct {
    kp_pairing_hint_t hint;
    uint32_t window_start;
    uint8_t failures, peer[7];
} kp_pairing_t;

void kp_pairing_reset(kp_pairing_t *state);
void kp_pairing_missing_key(kp_pairing_t *state);
// Count only unsecured, remote-terminated attempts with no local bond. Peer is
// the address type plus six address bytes. Other failures break the sequence.
void kp_pairing_disconnected(kp_pairing_t *state, uint32_t now,
                            const uint8_t peer[7], bool candidate);
const char *kp_pairing_title(kp_pairing_hint_t hint);
const char *kp_pairing_instructions(kp_pairing_hint_t hint);
