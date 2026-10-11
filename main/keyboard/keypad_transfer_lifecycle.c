#include "keypad_transfer_lifecycle.h"

void kp_transfer_lifecycle_enable(kp_transfer_lifecycle_t *s, uint64_t now) {
    if (s->phase != KP_TRANSFER_LISTEN) return;
    s->phase = KP_TRANSFER_WAIT;
    s->joining = true;
    s->zero_since_ms = now;
}

void kp_transfer_lifecycle_observe(kp_transfer_lifecycle_t *s, const kp_transfer_presence_t *p) {
    if (s->phase == KP_TRANSFER_LISTEN || !p->allocated) return;
    s->phase = p->online ? KP_TRANSFER_RUN : KP_TRANSFER_WAIT;
    s->joining = p->joining != 0;
    if (!p->online) s->zero_since_ms = p->zero_since_ms;
}

bool kp_transfer_lifecycle_expired(const kp_transfer_lifecycle_t *s, uint64_t now) {
    (void)now;
    return s->phase == KP_TRANSFER_WAIT && !s->joining;
}

void kp_transfer_lifecycle_reset(kp_transfer_lifecycle_t *s) {
    *s = (kp_transfer_lifecycle_t){0};
}
