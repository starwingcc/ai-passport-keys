#include "keypad_screen.h"
#include <stddef.h>

void kp_screen_defaults(kp_screen_config_t *c) {
    *c = (kp_screen_config_t){.version = KP_SCREEN_VERSION, .dim_after_s = 10,
        .off_after_s = 30, .brightness = 80, .dim_brightness = 20};
}
bool kp_screen_valid(const kp_screen_config_t *c) {
    return c && c->version == KP_SCREEN_VERSION && !c->reserved[0] && !c->reserved[1] &&
           c->brightness >= 10 && c->brightness <= 100 && c->brightness % 5 == 0 &&
           c->dim_brightness >= 5 && c->dim_brightness <= c->brightness && c->dim_brightness % 5 == 0 &&
           c->dim_after_s <= KP_SCREEN_DIM_MAX_S && c->dim_after_s % KP_SCREEN_TIME_STEP_S == 0 &&
           c->off_after_s <= KP_SCREEN_OFF_MAX_S && c->off_after_s % KP_SCREEN_TIME_STEP_S == 0 &&
           (!c->dim_after_s || !c->off_after_s || c->off_after_s > c->dim_after_s);
}
unsigned kp_screen_value(const kp_screen_config_t *c, kp_screen_setting_t setting) {
    switch (setting) {
    case KP_SCREEN_BRIGHTNESS: return c->brightness;
    case KP_SCREEN_DIM_AFTER: return c->dim_after_s;
    case KP_SCREEN_DIM_BRIGHTNESS: return c->dim_brightness;
    case KP_SCREEN_OFF_AFTER: return c->off_after_s;
    default: return 0;
    }
}
bool kp_screen_adjust(kp_screen_config_t *c, kp_screen_setting_t setting, int direction) {
    if (!kp_screen_valid(c) || setting >= KP_SCREEN_SETTING_COUNT || !direction) return false;
    kp_screen_config_t next = *c;
    if (setting == KP_SCREEN_BRIGHTNESS || setting == KP_SCREEN_DIM_BRIGHTNESS) {
        int value = (int)kp_screen_value(c, setting) + (direction > 0 ? 5 : -5);
        unsigned min = setting == KP_SCREEN_BRIGHTNESS ? 10 : 5;
        unsigned max = setting == KP_SCREEN_BRIGHTNESS ? 100 : c->brightness;
        if (value < (int)min || value > (int)max) return false;
        if (setting == KP_SCREEN_BRIGHTNESS) {
            next.brightness = value;
            if (next.dim_brightness > value) next.dim_brightness = value;
        } else next.dim_brightness = value;
    } else {
        unsigned max = setting == KP_SCREEN_DIM_AFTER ? KP_SCREEN_DIM_MAX_S : KP_SCREEN_OFF_MAX_S;
        unsigned count = max / KP_SCREEN_TIME_STEP_S + 1;
        unsigned index = kp_screen_value(c, setting) / KP_SCREEN_TIME_STEP_S;
        for (unsigned n = 0; n < count; ++n) {
            index = (index + (direction > 0 ? 1 : count - 1)) % count;
            unsigned value = index * KP_SCREEN_TIME_STEP_S;
            if (setting == KP_SCREEN_DIM_AFTER) next.dim_after_s = value;
            else next.off_after_s = value;
            if (kp_screen_valid(&next)) break;
        }
        if (kp_screen_value(&next, setting) == kp_screen_value(c, setting)) return false;
    }
    *c = next;
    return true;
}
void kp_screen_init(kp_screen_t *s, const kp_screen_config_t *c, uint32_t now) {
    *s = (kp_screen_t){.last_activity = now, .level = c->brightness, .state = KP_SCREEN_ACTIVE};
}
bool kp_screen_input(kp_screen_t *s, unsigned key, bool down, uint32_t now) {
    if (key >= 3) return false;
    uint8_t bit = 1U << key;
    bool was_down = (s->down_mask & bit) != 0;
    if (down) s->down_mask |= bit; else s->down_mask &= ~bit;
    if (was_down != down) {
        s->last_activity = now;
        if (down && s->state == KP_SCREEN_OFF) s->waking = true;
        s->state = KP_SCREEN_ACTIVE;
    }
    bool forward = !s->waking;
    if (!s->down_mask) s->waking = false;
    return forward;
}
void kp_screen_tick(kp_screen_t *s, const kp_screen_config_t *c, uint32_t now, bool inhibit) {
    if (s->down_mask || inhibit) s->last_activity = now;
    uint32_t idle = now - s->last_activity;
    kp_screen_state_t state = KP_SCREEN_ACTIVE;
    if (c->off_after_s && idle >= c->off_after_s * 1000U) state = KP_SCREEN_OFF;
    else if (c->dim_after_s && idle >= c->dim_after_s * 1000U) state = KP_SCREEN_DIM;
    if (state == KP_SCREEN_DIM) {
        if (s->state != state) { s->fade_started = now; s->fade_from = s->level; }
        uint32_t elapsed = now - s->fade_started;
        if (elapsed >= KP_SCREEN_FADE_MS) s->level = c->dim_brightness;
        else s->level = s->fade_from + ((int)c->dim_brightness - s->fade_from) * (int)elapsed / (int)KP_SCREEN_FADE_MS;
    } else s->level = state == KP_SCREEN_OFF ? 0 : c->brightness;
    s->state = state;
}
