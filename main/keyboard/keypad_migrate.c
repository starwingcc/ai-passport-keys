#include "keypad_migrate.h"
#include <string.h>

static bool text_valid(const kp_v2_config_t *c, unsigned offset, unsigned limit, bool multiline) {
    return offset < c->used && memchr(c->data + offset, 0, c->used - offset) &&
           kp_text_utf8_valid((const char *)c->data + offset, limit, multiline);
}
static bool copy_text(kp_config_t *out, const char *text, uint16_t *offset) {
    if (!*text) { *offset = 0; return true; }
    size_t size = strlen(text) + 1;
    if (size > sizeof(out->data) - out->used) return false;
    *offset = out->used; memcpy(out->data + out->used, text, size); out->used += size;
    return true;
}
bool kp_migrate_v2(const kp_v2_config_t *c, size_t size, kp_config_t *out) {
    if (!c || !out || size < offsetof(kp_v2_config_t, data) + 1 || size > sizeof(*c) ||
        c->version != 2 || !c->profile_count || c->profile_count > KP_MAX_PROFILES ||
        c->active_profile >= c->profile_count || !c->used || c->used > sizeof(c->data) ||
        size != offsetof(kp_v2_config_t, data) + c->used || c->data[0]) return false;
    // Validate all legacy offsets before rebuilding the smaller arena. Failed
    // validation cannot replace the defaults or an already loaded configuration.
    for (unsigned p = 0; p < c->profile_count; ++p) {
        const kp_v2_profile_t *profile = &c->profiles[p];
        if (!text_valid(c, profile->name, KP_NAME_CHARS, false) || !c->data[profile->name]) return false;
        for (unsigned j = 0; j < p; ++j)
            if (!strcmp((const char *)c->data + profile->name, (const char *)c->data + c->profiles[j].name)) return false;
        for (unsigned i = 0; i < KP_BINDINGS; ++i) {
            const kp_v2_binding_t *b = &profile->bindings[i];
            if (b->reserved || b->count > KP_MAX_STEPS || !text_valid(c, b->title, KP_NAME_CHARS, false) ||
                !text_valid(c, b->description, 64, true) || (!b->count && b->steps) ||
                (b->count && (b->steps & 1)) || b->steps + b->count * sizeof(kp_step_t) > c->used) return false;
            for (unsigned j = 0; j < b->count; ++j) {
                kp_step_t step; memcpy(&step, c->data + b->steps + j * sizeof(step), sizeof(step));
                if (!kp_action_valid(&step.action, false) || step.delay_ms > KP_MAX_DELAY_MS ||
                    (j + 1 == b->count && step.delay_ms)) return false;
            }
        }
    }
    kp_config_empty(out);
    out->profile_count = c->profile_count;
    // Restore validated names verbatim, including characters supported by an
    // older font inventory. Public input setters enforce today's glyph coverage.
    for (unsigned p = 0; p < c->profile_count; ++p) {
        const kp_v2_profile_t *profile = &c->profiles[p];
        if (!copy_text(out, (const char *)c->data + profile->name, &out->profiles[p].name)) return false;
        for (unsigned i = 0; i < KP_BINDINGS; ++i) {
            const kp_v2_binding_t *b = &profile->bindings[i];
            kp_binding_t *target = &out->profiles[p].bindings[i];
            if (!copy_text(out, (const char *)c->data + b->title, &target->title)) return false;
            if (b->count) {
                unsigned offset = (out->used + 1U) & ~1U;
                size_t bytes = b->count * sizeof(kp_step_t);
                if (offset + bytes > sizeof(out->data)) return false;
                target->steps = offset; target->count = b->count;
                memcpy(out->data + offset, c->data + b->steps, bytes); out->used = offset + bytes;
            }
        }
    }
    out->active_profile = c->active_profile;
    return kp_config_valid(out);
}
