#include "keypad_web_presence.h"
#include <assert.h>
#include <stdio.h>

static void boundaries(uint64_t start) {
    kp_web_presence_t s;
    kp_web_presence_init(&s, start);
    assert(!kp_web_presence_expired(&s, start + 59999));
    assert(kp_web_presence_expired(&s, start + 60000));
    uint32_t a = kp_web_presence_join(&s, start + 60000);
    assert(a && !kp_web_presence_expired(&s, start + 60000));
    uint32_t b = kp_web_presence_join(&s, start + 60001);
    kp_web_presence_leave(&s, a, start + 61000);
    assert(s.online == 1);
    assert(kp_web_presence_ping(&s, b, start + 170000));
    kp_web_presence_leave(&s, b, start + 170001);
    kp_web_presence_leave(&s, b, start + 200000); // Idempotent leave cannot reset grace.
    assert(!kp_web_presence_expired(&s, start + 230000));
    assert(kp_web_presence_expired(&s, start + 230001));
    a = kp_web_presence_join(&s, start + 230001);
    assert(!kp_web_presence_expired(&s, start + 410000));
    assert(kp_web_presence_expired(&s, start + 410001)); // 120s lease + 60s grace.
    assert(!kp_web_presence_ping(&s, a, start + 410001));
    // Pauses preserve both lease and quiet-window time, including joins while paused.
    kp_web_presence_init(&s, start);
    kp_web_presence_pause(&s, true, start + 59999);
    assert(!kp_web_presence_expired(&s, start + 999999));
    kp_web_presence_pause(&s, false, start + 1000000);
    assert(!kp_web_presence_expired(&s, start + 1000000));
    assert(kp_web_presence_expired(&s, start + 1000001));
    a = kp_web_presence_join(&s, start + 1000001);
    kp_web_presence_pause(&s, true, start + 1000002);
    b = kp_web_presence_join(&s, start + 1400000);
    assert(kp_web_presence_ping(&s, a, start + 1500000));
    kp_web_presence_pause(&s, false, start + 1600000);
    assert(s.online == 2 && !kp_web_presence_expired(&s, start + 1779999));
    assert(kp_web_presence_expired(&s, start + 1780000));
    (void)b;
}
int main(void) {
    boundaries(0);
    boundaries((uint64_t)UINT32_MAX - 100);
    boundaries(UINT64_MAX - 100);
    kp_web_presence_t s;
    kp_web_presence_init(&s, 0);
    for (unsigned i = 0; i < KP_WEB_PAGES; ++i) assert(kp_web_presence_join(&s, i));
    assert(!kp_web_presence_join(&s, 10) && s.online == 4);
    // Late polling uses the last actual expiration, not polling time.
    assert(kp_web_presence_expired(&s, 180003));
    assert(s.zero_since_ms == 120003);
    puts("Web presence: four-page capacity, heartbeat/leave, last expiration, exact 60s, operation pauses and 64-bit wrap: PASS");
}
