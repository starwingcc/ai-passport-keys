#include "keypad_pairing.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    kp_pairing_t state = {0};
    uint8_t mac[7] = {0,1,2,3,4,5,6}, other[7] = {0,7,8,9,10,11,12};
    // Idle and a single failed first pairing must never suggest stale keys.
    assert(state.hint == KP_PAIRING_NONE);
    kp_pairing_disconnected(&state, 100, mac, true);
    assert(state.hint == KP_PAIRING_NONE);
    kp_pairing_disconnected(&state, 200, mac, false);
    assert(state.failures == 0);
    // Four failures from the same peer within ten seconds produce only a
    // tentative diagnosis. Other peers and sparse failures cannot accumulate.
    for (unsigned i = 0; i < 3; ++i) kp_pairing_disconnected(&state, 300+i*100, mac, true);
    kp_pairing_disconnected(&state, 600, other, true);
    assert(state.failures == 1 && state.hint == KP_PAIRING_NONE);
    kp_pairing_disconnected(&state, 11000, other, true);
    assert(state.failures == 1);
    for (unsigned i = 0; i < 3; ++i) kp_pairing_disconnected(&state, 11100+i*100, other, true);
    assert(state.hint == KP_PAIRING_RETRY_FAILED);
    assert(strstr(kp_pairing_instructions(state.hint), "可能"));
    // The notice stays stable through retry/disconnect noise. Direct missing
    // key evidence upgrades it, and can never be downgraded by the heuristic.
    kp_pairing_disconnected(&state, 50000, mac, false);
    assert(state.hint == KP_PAIRING_RETRY_FAILED);
    kp_pairing_missing_key(&state);
    for (unsigned i = 0; i < 6; ++i) kp_pairing_disconnected(&state, 60000+i*100, mac, true);
    assert(state.hint == KP_PAIRING_KEY_MISSING);
    assert(!strstr(kp_pairing_instructions(state.hint), "可能"));
    // A successful secure link resets everything; millisecond wrap is safe.
    kp_pairing_reset(&state);
    assert(state.hint == KP_PAIRING_NONE && !state.failures);
    for (unsigned i = 0; i < 4; ++i) kp_pairing_disconnected(&state, UINT32_MAX-150+i*100, mac, true);
    assert(state.hint == KP_PAIRING_RETRY_FAILED);
    puts("Keypad pairing recovery classification, persistence and timer wrap: PASS");
}
