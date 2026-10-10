#include "keypad_transfer_lifecycle.h"
#include <assert.h>
#include <stdio.h>
static void boundaries(uint64_t start) {
    kp_transfer_lifecycle_t s = {0};
    kp_transfer_lifecycle_enable(&s, start);
    assert(s.phase == KP_TRANSFER_WAIT);
    assert(!kp_transfer_lifecycle_expired(&s, start + 59999));
    // Repeated visits/resources do not renew an already-enabled window.
    kp_transfer_lifecycle_enable(&s, start + 59999);
    assert(kp_transfer_lifecycle_expired(&s, start + 60000));
    kp_transfer_presence_t p = {.allocated=true, .online=1, .generation=1};
    kp_transfer_lifecycle_observe(&s, &p);
    assert(s.phase == KP_TRANSFER_RUN && !kp_transfer_lifecycle_expired(&s, start + 900000));
    p.online=0; p.zero_since_ms=start + 900001;
    kp_transfer_lifecycle_observe(&s, &p);
    assert(!kp_transfer_lifecycle_expired(&s, p.zero_since_ms + 59999));
    assert(kp_transfer_lifecycle_expired(&s, p.zero_since_ms + 60000));
    p.online=1; kp_transfer_lifecycle_observe(&s, &p);
    assert(!kp_transfer_lifecycle_expired(&s, p.zero_since_ms + 60000));
    // A join and close between samples is preserved by the HTTP snapshot.
    p.online=0; p.zero_since_ms=start + 1100000; ++p.generation;
    kp_transfer_lifecycle_observe(&s, &p);
    p.zero_since_ms=start + 1200000;
    kp_transfer_lifecycle_observe(&s, &p);
    assert(!kp_transfer_lifecycle_expired(&s, start + 1259999));
    assert(kp_transfer_lifecycle_expired(&s, start + 1260000));
    kp_transfer_lifecycle_reset(&s);
    kp_transfer_lifecycle_observe(&s, &p);
    assert(s.phase == KP_TRANSFER_LISTEN);
    kp_transfer_lifecycle_enable(&s, start + 1400000);
    assert(!kp_transfer_lifecycle_expired(&s, start + 1459999));
}
int main(void) {
    boundaries(0); // Timestamp zero is a valid deadline anchor.
    boundaries(UINT32_MAX - 100); // No narrowing at the 49-day boundary.
    boundaries(UINT64_MAX - 100); // Synthetic unsigned monotonic wrap.
    puts("Transfer idle: first no-join window, exact 60s boundary, online cancellation, sampled join/leave, independent clock and wrap: PASS");
}
