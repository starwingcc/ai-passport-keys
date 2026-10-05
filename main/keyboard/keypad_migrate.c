#include "keypad_migrate.h"
#include <string.h>

static bool text_valid(const uint8_t *data, unsigned used, unsigned offset, unsigned limit, bool multiline) {
    return offset < used && memchr(data + offset, 0, used - offset) &&
           kp_text_utf8_valid((const char *)data + offset, limit, multiline);
}
static bool binding_valid(const uint8_t *data, unsigned used, unsigned title, unsigned steps,
                          unsigned count, unsigned reserved) {
    if (reserved || count > KP_MAX_STEPS || !text_valid(data, used, title, KP_NAME_CHARS, false) ||
        (!count && steps) || (count && (!steps || (steps & 1))) ||
        steps + count * sizeof(kp_step_t) > used) return false;
    for (unsigned i = 0; i < count; ++i) {
        kp_step_t step; memcpy(&step, data + steps + i * sizeof(step), sizeof(step));
        if (!kp_action_valid(&step.action, false) ||
            (step.action.type == KP_KEYBOARD && step.action.consumer) || step.delay_ms > KP_MAX_DELAY_MS ||
            (i + 1 == count && step.delay_ms)) return false;
    }
    return true;
}
static unsigned slot(unsigned old) { return (old / 3) * 4 + (old % 3 == 2 ? 3 : old % 3); }
static void arena(kp_config_t *out, const uint8_t *data, unsigned used, unsigned profiles, unsigned active) {
    kp_config_empty(out); out->used = used; out->profile_count = profiles; out->active_profile = active;
    // Copy validated data verbatim, preserving shared offsets and old glyphs.
    // The smaller v4 header guarantees capacity even for a full v2/v3 arena.
    // v2 descriptions are no longer referenced by the new descriptors.
    memcpy(out->data, data, used);
}
bool kp_migrate_v2(const kp_v2_config_t *c, size_t size, kp_config_t *out) {
    if (!c || !out || size < 537 || size > sizeof(*c) || c->version != 2 ||
        !c->profile_count || c->profile_count > 8 || c->active_profile >= c->profile_count ||
        !c->used || c->used > sizeof(c->data) || size != 536U + c->used || c->data[0]) return false;
    for (unsigned p = 0; p < c->profile_count; ++p) {
        const kp_v2_profile_t *profile = &c->profiles[p];
        if (!text_valid(c->data, c->used, profile->name, KP_NAME_CHARS, false) || !c->data[profile->name]) return false;
        for (unsigned j = 0; j < p; ++j)
            if (!strcmp((const char *)c->data + profile->name, (const char *)c->data + c->profiles[j].name)) return false;
        for (unsigned i = 0; i < 8; ++i) {
            const kp_v2_binding_t *b = &profile->bindings[i];
            if (!binding_valid(c->data, c->used, b->title, b->steps, b->count, b->reserved) ||
                !text_valid(c->data, c->used, b->description, 64, true)) return false;
        }
    }
    arena(out, c->data, c->used, c->profile_count, c->active_profile);
    for (unsigned p = 0; p < c->profile_count; ++p) {
        out->profiles[p].name = c->profiles[p].name;
        for (unsigned i = 0; i < 8; ++i) {
            const kp_v2_binding_t *b = &c->profiles[p].bindings[i];
            out->profiles[p].bindings[slot(i)] = (kp_binding_t){.title = b->title, .steps = b->steps / 2, .count = b->count};
        }
    }
    return true;
}
bool kp_migrate_v3(const kp_v3_config_t *c, size_t size, kp_config_t *out) {
    if (!c || !out || size < 409 || size > sizeof(*c) || c->version != 3 ||
        !c->profile_count || c->profile_count > 8 || c->active_profile >= c->profile_count ||
        !c->used || c->used > sizeof(c->data) || size != 408U + c->used || c->data[0]) return false;
    for (unsigned p = 0; p < c->profile_count; ++p) {
        const kp_v3_profile_t *profile = &c->profiles[p];
        if (!text_valid(c->data, c->used, profile->name, KP_NAME_CHARS, false) || !c->data[profile->name]) return false;
        for (unsigned j = 0; j < p; ++j)
            if (!strcmp((const char *)c->data + profile->name, (const char *)c->data + c->profiles[j].name)) return false;
        for (unsigned i = 0; i < 8; ++i) {
            const kp_v3_binding_t *b = &profile->bindings[i];
            if (!binding_valid(c->data, c->used, b->title, b->steps, b->count, b->reserved)) return false;
        }
    }
    arena(out, c->data, c->used, c->profile_count, c->active_profile);
    for (unsigned p = 0; p < c->profile_count; ++p) {
        out->profiles[p].name = c->profiles[p].name;
        for (unsigned i = 0; i < 8; ++i) {
            const kp_v3_binding_t *b = &c->profiles[p].bindings[i];
            out->profiles[p].bindings[slot(i)] = (kp_binding_t){.title = b->title, .steps = b->steps / 2, .count = b->count};
        }
    }
    return true;
}
bool kp_migrate_v4(kp_config_t *c, size_t size) {
    if (!c || c->version != 4 || size < offsetof(kp_config_t, data) + 1 ||
        size > sizeof(*c) || size != kp_config_size(c)) return false;
    c->version = KP_VERSION;
    bool valid = kp_config_valid(c);
    for (unsigned p = 0; valid && p < c->profile_count; ++p)
        for (unsigned i = 0; valid && i < KP_BINDINGS; ++i) {
            const kp_binding_t *b = &c->profiles[p].bindings[i];
            for (unsigned j = 0; valid && j < b->count; ++j) {
                kp_step_t step;
                valid = kp_binding_step(c, b, j, &step) &&
                        (step.action.type != KP_KEYBOARD || !step.action.consumer);
            }
        }
    if (!valid) c->version = 4;
    return valid;
}
