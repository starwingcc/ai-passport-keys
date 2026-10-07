#include "keypad_runner.h"
#include <string.h>
bool kp_runner_start(kp_runner_t *r, const kp_config_t *c, const kp_binding_t *b, bool hold, kp_system_t system) {
    if (r->running || !b || !b->count || b->count > KP_MAX_STEPS || (unsigned)system >= KP_SYSTEM_COUNT) return false;
    memset(r, 0, sizeof(*r)); r->config = c; r->binding = *b;
    r->system = system;
    for (unsigned i = 0; i < b->count; ++i) {
        kp_step_t step;
        if (!kp_binding_step(c, b, i, &step) || !kp_action_valid(&step.action, false)) return false;
        if (kp_action_applies(&step.action, system)) ++r->total;
    }
    if (!r->total) return false;
    r->running = true; r->hold = hold && r->total == 1;
    return true;
}
bool kp_runner_request(kp_runner_t *r, uint32_t now, kp_action_t *action) {
    if (!r->running || r->pending || r->phase == KP_RUN_HOLD) return false;
    if (r->phase != KP_RUN_READY && (int32_t)(now - r->deadline) < 0) return false;
    if (r->phase == KP_RUN_WAIT) { ++r->index; r->phase = KP_RUN_READY; }
    memset(action, 0, sizeof(*action));
    if (r->phase == KP_RUN_READY) {
        kp_step_t step;
        for (;;) {
            if (!kp_binding_step(r->config, &r->binding, r->index, &step)) { r->failed = true; r->running = false; return false; }
            if (kp_action_applies(&step.action, r->system)) break;
            ++r->index; // Skipped actions introduce neither HID reports nor waits.
        }
        *action = step.action;
    }
    r->pending = true; return true;
}
void kp_runner_result(kp_runner_t *r, bool success, uint32_t now) {
    if (!r->pending) return;
    r->pending = false;
    if (!success) { r->failed = true; r->running = false; return; }
    if (r->phase == KP_RUN_READY) {
        r->phase = r->hold ? KP_RUN_HOLD : KP_RUN_PRESSED; r->deadline = now + KP_PULSE_MS;
    } else {
        ++r->completed;
        if (r->completed == r->total) r->running = false;
        else {
            kp_step_t step; (void)kp_binding_step(r->config, &r->binding, r->index, &step);
            r->phase = KP_RUN_WAIT; r->deadline = now + step.delay_ms;
        }
    }
}
void kp_runner_release(kp_runner_t *r, uint32_t now) {
    if (r->running && r->hold && r->phase == KP_RUN_READY) r->hold = false;
    if (r->running && r->phase == KP_RUN_HOLD) { r->phase = KP_RUN_PRESSED; r->deadline = now; }
}
void kp_runner_cancel(kp_runner_t *r) { r->running = r->pending = false; }
