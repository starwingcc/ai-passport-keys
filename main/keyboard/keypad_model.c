#include "keypad_model.h"
#include <stdio.h>
#include <string.h>

static const uint16_t media[] = {0xe9, 0xea, 0xe2, 0xcd, 0xb5, 0xb6};
bool kp_key_valid(unsigned key) {
    return (key >= 4 && key <= 0x73) && key != 0x66;
}
void kp_config_empty(kp_config_t *c) {
    memset(c, 0, sizeof(*c)); c->version = KP_VERSION; c->used = 1;
}
size_t kp_config_size(const kp_config_t *c) { return offsetof(kp_config_t, data) + c->used; }
const char *kp_text(const kp_config_t *c, unsigned offset) {
    return offset < c->used ? (const char *)c->data + offset : "";
}
static bool put_text(kp_config_t *c, const char *text, uint16_t *offset) {
    if (!*text) { *offset = 0; return true; }
    size_t size = strlen(text) + 1;
    if (size > sizeof(c->data) - c->used) return false;
    *offset = c->used; memcpy(c->data + c->used, text, size); c->used += size;
    return true;
}
bool kp_profile_add(kp_config_t *c, const char *name) {
    if (c->profile_count >= KP_MAX_PROFILES || !name || !*name || !kp_text_valid(name, KP_NAME_CHARS, false)) return false;
    for (unsigned i = 0; i < c->profile_count; ++i)
        if (!strcmp(name, kp_text(c, c->profiles[i].name))) return false;
    if (!put_text(c, name, &c->profiles[c->profile_count].name)) return false;
    ++c->profile_count; return true;
}
bool kp_profile_name_set(kp_config_t *c, unsigned profile, const char *name) {
    return profile < c->profile_count && name && *name && kp_text_valid(name, KP_NAME_CHARS, false) &&
           put_text(c, name, &c->profiles[profile].name);
}
bool kp_action_valid(const kp_action_t *a, bool allow_none) {
    if (!a || a->type > KP_MEDIA) return false;
    bool any = a->modifiers != 0;
    for (unsigned i = 0; i < 6; ++i) {
        if (!a->keys[i]) continue;
        if (a->type != KP_KEYBOARD || !kp_key_valid(a->keys[i])) return false;
        any = true;
        for (unsigned j = 0; j < i; ++j) if (a->keys[j] == a->keys[i]) return false;
    }
    if (a->type == KP_KEYBOARD)
        return (any || a->logical_modifiers) && !(a->logical_modifiers & ~3U);
    if (a->type == KP_MEDIA) {
        for (unsigned i = 0; i < sizeof(media) / sizeof(media[0]); ++i)
            if (a->consumer == media[i]) return !any;
        return false;
    }
    return allow_none && !any && !a->consumer;
}
bool kp_action_resolve(const kp_action_t *a, kp_system_t system, kp_action_t *out) {
    if (!out || (unsigned)system >= KP_SYSTEM_COUNT || !kp_action_valid(a, true)) return false;
    *out = *a;
    if (a->type == KP_KEYBOARD) {
        if (a->logical_modifiers & KP_CMD_CTRL_LEFT)
            out->modifiers |= system == KP_SYSTEM_MACOS ? 0x08 : 0x01;
        if (a->logical_modifiers & KP_CMD_CTRL_RIGHT)
            out->modifiers |= system == KP_SYSTEM_MACOS ? 0x80 : 0x10;
        out->logical_modifiers = 0;
    }
    return true;
}
bool kp_binding_set(kp_config_t *c, unsigned profile, unsigned slot,
                    const char *title, const kp_step_t *steps, unsigned count) {
    if (profile >= c->profile_count || slot >= KP_BINDINGS || count > KP_MAX_STEPS ||
        (count && !steps) || !kp_text_valid(title, KP_NAME_CHARS, false)) return false;
    for (unsigned i = 0; i < count; ++i)
        if (!kp_action_valid(&steps[i].action, false) || steps[i].delay_ms > KP_MAX_DELAY_MS ||
            (i + 1 == count && steps[i].delay_ms)) return false;
    uint16_t before = c->used; kp_binding_t b = {0};
    uint16_t title_offset;
    if (!put_text(c, title, &title_offset)) goto fail;
    b.title = title_offset;
    if (count) {
        unsigned offset = (c->used + 1U) & ~1U;
        if (offset + count * sizeof(*steps) > sizeof(c->data)) goto fail;
        b.steps = offset / 2; b.count = count;
        memcpy(c->data + offset, steps, count * sizeof(*steps)); c->used = offset + count * sizeof(*steps);
    }
    c->profiles[profile].bindings[slot] = b; return true;
fail:
    memset(c->data + before, 0, c->used - before); c->used = before; return false;
}
const kp_binding_t *kp_binding(const kp_config_t *c, unsigned slot) {
    return slot < KP_BINDINGS && c->active_profile < c->profile_count ? &c->profiles[c->active_profile].bindings[slot] : NULL;
}
bool kp_binding_step(const kp_config_t *c, const kp_binding_t *b, unsigned index, kp_step_t *out) {
    if (!b || !out || index >= b->count || b->steps * 2U + (index + 1) * sizeof(*out) > c->used) return false;
    memcpy(out, c->data + b->steps * 2U + index * sizeof(*out), sizeof(*out)); return true;
}
static bool stored_text(const kp_config_t *c, unsigned offset, unsigned limit, bool multiline) {
    return offset < c->used && memchr(c->data + offset, 0, c->used - offset) &&
           kp_text_utf8_valid(kp_text(c, offset), limit, multiline);
}
bool kp_config_valid(const kp_config_t *c) {
    if (!c || c->version != KP_VERSION || !c->profile_count || c->profile_count > KP_MAX_PROFILES ||
        c->active_profile >= c->profile_count || !c->used || c->used > sizeof(c->data) || c->data[0]) return false;
    for (unsigned p = 0; p < c->profile_count; ++p) {
        const kp_profile_t *profile = &c->profiles[p];
        if (!stored_text(c, profile->name, KP_NAME_CHARS, false) || !*kp_text(c, profile->name)) return false;
        for (unsigned j = 0; j < p; ++j)
            if (!strcmp(kp_text(c, profile->name), kp_text(c, c->profiles[j].name))) return false;
        for (unsigned i = 0; i < KP_BINDINGS; ++i) {
            const kp_binding_t *b = &profile->bindings[i];
            if (b->count > KP_MAX_STEPS ||
                !stored_text(c, b->title, KP_NAME_CHARS, false) ||
                (!b->count && b->steps) || (b->count && !b->steps)) return false;
            for (unsigned j = 0; j < b->count; ++j) {
                kp_step_t step;
                if (!kp_binding_step(c, b, j, &step) || !kp_action_valid(&step.action, false) ||
                    step.delay_ms > KP_MAX_DELAY_MS || (j + 1 == b->count && step.delay_ms)) return false;
            }
        }
    }
    return true;
}
bool kp_legacy_valid(const kp_legacy_config_t *c) {
    if (!c || c->version != 1) return false;
    for (unsigned i = 0; i < KP_LEGACY_BINDINGS; ++i)
        if (!kp_action_valid(&c->bindings[i], true) ||
            (c->bindings[i].type == KP_KEYBOARD && c->bindings[i].consumer)) return false;
    return true;
}
bool kp_migrate(const kp_legacy_config_t *old, kp_config_t *c) {
    if (!kp_legacy_valid(old)) return false;
    kp_config_empty(c);
    if (!kp_profile_add(c, "默认")) return false;
    for (unsigned i = 0; i < KP_LEGACY_BINDINGS; ++i) {
        kp_step_t step = {.action = old->bindings[i]};
        bool enter = step.action.type == KP_KEYBOARD && !step.action.modifiers && step.action.keys[0] == 0x28;
        for (unsigned j = 1; j < 6; ++j) if (step.action.keys[j]) enter = false;
        const char *title = enter ? "回车" : "";
        unsigned slot = (i / 3) * 4 + (i % 3 == 2 ? 3 : i % 3);
        if (!kp_binding_set(c, 0, slot, title, &step, step.action.type == KP_NONE ? 0 : 1)) return false;
    }
    return true;
}
void kp_defaults(kp_config_t *c) {
    kp_legacy_config_t old = {.version = 1};
    old.bindings[0] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x29}};
    old.bindings[2] = (kp_action_t){.type = KP_KEYBOARD, .modifiers = 0x04};
    old.bindings[3] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x2a}};
    old.bindings[4] = (kp_action_t){.type = KP_KEYBOARD, .modifiers = 0x08, .keys = {0x04}};
    old.bindings[5] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x51}};
    old.bindings[6] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x28}};
    (void)kp_migrate(&old, c);
}
void kp_report(const kp_action_t *a, uint8_t keyboard[8], uint8_t consumer[2]) {
    memset(keyboard, 0, 8);
    memset(consumer, 0, 2);
    if (!a) return;
    if (a->type == KP_KEYBOARD) {
        if (a->logical_modifiers) return; // Only resolved actions reach HID.
        keyboard[0] = a->modifiers;
        memcpy(keyboard + 2, a->keys, 6);
    } else if (a->type == KP_MEDIA) {
        consumer[0] = a->consumer & 0xff;
        consumer[1] = a->consumer >> 8;
    }
}
static void append(char *out, size_t size, const char *word) {
    size_t len = strlen(out);
    if (len < size) snprintf(out + len, size - len, "%s%s", len ? "+" : "", word);
}
void kp_action_label(const kp_action_t *a, char *out, size_t size) {
    if (!size) return;
    out[0] = 0;
    if (a->type == KP_NONE) { snprintf(out, size, "无"); return; }
    if (a->type == KP_MEDIA) {
        const char *labels[] = {"音量加", "音量减", "静音", "播放暂停", "下一首", "上一首"};
        for (unsigned i = 0; i < 6; ++i) if (media[i] == a->consumer) snprintf(out, size, "%s", labels[i]);
        return;
    }
    const char *mods[] = {"Control", "Shift", "Option", "Command", "RControl", "RShift", "ROption", "RCommand"};
    if (a->logical_modifiers & KP_CMD_CTRL_LEFT) append(out, size, "Cmd/Ctrl");
    if (a->logical_modifiers & KP_CMD_CTRL_RIGHT) append(out, size, "RCmd/Ctrl");
    for (unsigned i = 0; i < 8; ++i) if (a->modifiers & (1U << i)) append(out, size, mods[i]);
    for (unsigned i = 0; i < 6; ++i) {
        unsigned k = a->keys[i];
        if (!k) continue;
        char name[16];
        if (k >= 4 && k <= 29) snprintf(name, sizeof(name), "%c", 'A' + k - 4);
        else if (k >= 30 && k <= 39) snprintf(name, sizeof(name), "%u", (k - 29) % 10);
        else if (k >= 0x3a && k <= 0x45) snprintf(name, sizeof(name), "F%u", k - 0x39);
        else if (k >= 0x68 && k <= 0x73) snprintf(name, sizeof(name), "F%u", k - 0x68 + 13);
        else {
            const char *s = NULL;
            switch (k) {
            case 0x28: s = "Enter"; break; case 0x29: s = "Esc"; break;
            case 0x2a: s = "Backspace"; break; case 0x2b: s = "Tab"; break;
            case 0x2c: s = "Space"; break; case 0x4c: s = "Delete"; break;
            case 0x4f: s = "Right"; break; case 0x50: s = "Left"; break;
            case 0x51: s = "Down"; break; case 0x52: s = "Up"; break;
            default: break;
            }
            if (s) snprintf(name, sizeof(name), "%s", s);
            else snprintf(name, sizeof(name), "Key%02X", k);
        }
        append(out, size, name);
    }
}
void kp_binding_label(const kp_config_t *c, const kp_binding_t *b, char *out, size_t size) {
    if (!size) return;
    out[0] = 0;
    if (!b || !b->count) { snprintf(out, size, "无"); return; }
    for (unsigned i = 0; i < b->count; ++i) {
        kp_step_t step; char action[192];
        if (!kp_binding_step(c, b, i, &step)) return;
        kp_action_label(&step.action, action, sizeof(action));
        size_t used = strlen(out), needed = strlen(action) + (i ? 5 : 0);
        if (used + needed + 1 > size) {
            if (used + 4 <= size) memcpy(out + used, "…", 4);
            return;
        }
        snprintf(out + used, size - used, "%s%s", i ? " → " : "", action);
    }
}
void kp_binding_title(const kp_config_t *c, const kp_binding_t *b, char *out, size_t size) {
    if (b && *kp_text(c, b->title)) snprintf(out, size, "%s", kp_text(c, b->title));
    else if (b && b->count > 1) snprintf(out, size, "组合指令");
    else kp_binding_label(c, b, out, size);
}
static void emit(kp_model_t *m, kp_effect_t e, const kp_action_t *a) {
    if (m->emit) m->emit(m->user, e, a);
}
void kp_cancel(kp_model_t *m) {
    memset(m->key, 0, sizeof(m->key));
    m->blocked = m->down_mask != 0;
    m->executing = false; m->feedback.visible = false;
    emit(m, KP_CANCEL, NULL);
}
void kp_link_changed(kp_model_t *m, uint32_t now) {
    kp_feedback_t previous = m->feedback;
    bool running = m->executing;
    bool show = m->page == KP_CONSOLE && previous.visible &&
                (running || (previous.state == KP_FEEDBACK_FAILED && (int32_t)(now - previous.until) < 0));
    kp_cancel(m);
    if (show) {
        previous.state = KP_FEEDBACK_FAILED; previous.holding = false;
        if (running) previous.until = now + KP_FEEDBACK_MS;
        m->feedback = previous;
    }
}
static void page(kp_model_t *m, kp_page_t target) {
    kp_cancel(m);
    m->page = target;
    m->selected = 0;
}
void kp_set_busy(kp_model_t *m, bool busy) {
    m->executing = busy;
    if (!busy) return;
    for (unsigned i = 0; i < 3; ++i) {
        m->key[i].pending = false;
        if (m->key[i].down && i != m->triggered_slot / 4) m->key[i].ignored = true;
    }
}
void kp_init(kp_model_t *m, const kp_config_t *c, kp_emit_t cb, void *u) {
    memset(m, 0, sizeof(*m));
    if (kp_config_valid(c)) m->config = *c; else kp_defaults(&m->config);
    m->emit = cb; m->user = u; m->page = KP_CONSOLE;
    kp_screen_defaults(&m->screen_config);
    kp_screen_init(&m->screen, &m->screen_config, 0);
}
void kp_screen_saved(kp_model_t *m, bool success) {
    if (!m->screen_saving) return;
    m->screen_saving = false; m->screen_save_failed = !success;
    if (success) {
        m->screen_config = m->screen_draft;
        page(m, KP_SCREEN); m->selected = m->screen_setting;
    }
    ++m->config_revision;
}
uint8_t kp_screen_level(const kp_model_t *m) {
    if (m->page == KP_SCREEN_EDIT &&
        (m->screen_setting == KP_SCREEN_BRIGHTNESS || m->screen_setting == KP_SCREEN_DIM_BRIGHTNESS))
        return kp_screen_value(&m->screen_draft, m->screen_setting);
    return m->screen.level;
}
void kp_system_saved(kp_model_t *m, bool success) {
    if (!m->system_saving) return;
    m->system_saving = false; m->system_save_failed = !success; m->system_saved = success;
    if (success) m->system = m->selected;
    ++m->config_revision;
}
void kp_hotspot_exit(kp_model_t *m) {
    if (m->page != KP_HOTSPOT) return;
    page(m, KP_SETTINGS);
    emit(m, KP_AP_STOP, NULL);
}
static void back(kp_model_t *m) {
    switch (m->page) {
    case KP_CONSOLE: case KP_SETTINGS: page(m, KP_MENU); break;
    case KP_HOTSPOT: kp_hotspot_exit(m); break;
    case KP_FORGET: case KP_AP_CONFIRM: page(m, KP_SETTINGS); break;
    case KP_PROFILES: page(m, KP_MENU); break;
    case KP_SCREEN: page(m, KP_SETTINGS); break;
    case KP_SYSTEM:
        if (!m->system_saving) page(m, KP_SETTINGS);
        break;
    case KP_SCREEN_EDIT:
        if (!m->screen_saving) { page(m, KP_SCREEN); m->selected = m->screen_setting; }
        break;
    case KP_MENU: kp_cancel(m); break;
    }
}
static void click(kp_model_t *m, unsigned key, unsigned clicks) {
    if (m->page == KP_CONSOLE) {
        if (m->executing) return;
        m->triggered_slot = key * 4 + clicks - 1;
        kp_step_t step;
        if (kp_binding_step(&m->config, kp_binding(&m->config, m->triggered_slot), 0, &step)) emit(m, KP_TAP, &step.action);
        return;
    }
    if (m->page == KP_HOTSPOT) {
        // Only long OK exits; short keys cannot accidentally stop configuration.
    } else if (m->page == KP_SYSTEM) {
        if (m->system_saving) return;
        if (key < 2) {
            m->selected = (m->selected + 1) % KP_SYSTEM_COUNT;
            m->system_saved = m->system_save_failed = false;
        } else {
            // Cancel the macro and release keys before requesting persistence.
            kp_cancel(m); m->system_saving = true; ++m->config_revision;
            emit(m, KP_SAVE_SYSTEM, NULL);
        }
    } else if (m->page == KP_SCREEN_EDIT) {
        if (m->screen_saving) return;
        if (key < 2) {
            if (kp_screen_adjust(&m->screen_draft, m->screen_setting, key == 0 ? 1 : -1)) {
                m->screen_save_failed = false; ++m->config_revision;
            }
        } else {
            m->screen_saving = true; ++m->config_revision;
            emit(m, KP_SAVE_SCREEN, NULL);
        }
    } else if (key < 2) {
        unsigned count = m->page == KP_SETTINGS ? 4 : m->page == KP_MENU ? 3 :
                         m->page == KP_SCREEN ? KP_SCREEN_SETTING_COUNT : m->page == KP_PROFILES ? m->config.profile_count : 2;
        m->selected = (m->selected + (key == 0 ? count - 1 : 1)) % count;
    } else if (m->page == KP_MENU) {
        kp_page_t target = m->selected == 0 ? KP_CONSOLE : m->selected == 1 ? KP_PROFILES : KP_SETTINGS;
        page(m, target);
        if (target == KP_PROFILES) m->selected = m->config.active_profile;
    } else if (m->page == KP_PROFILES) emit(m, KP_SELECT_PROFILE, NULL);
    else if (m->page == KP_SETTINGS) {
        kp_page_t target = m->selected == 0 ? KP_SCREEN : m->selected == 1 ? KP_SYSTEM :
                           m->selected == 2 ? KP_AP_CONFIRM : KP_FORGET;
        page(m, target);
        if (target == KP_SYSTEM) {
            m->selected = m->system;
            m->system_saved = m->system_save_failed = false;
        }
    } else if (m->page == KP_SCREEN) {
        m->screen_setting = m->selected;
        m->screen_draft = m->screen_config; m->screen_save_failed = false;
        page(m, KP_SCREEN_EDIT);
    } else if (m->page == KP_AP_CONFIRM) {
        bool confirm = m->selected == 1;
        page(m, confirm ? KP_HOTSPOT : KP_SETTINGS);
        if (confirm) emit(m, KP_AP_START, NULL);
    } else if (m->page == KP_FORGET) {
        bool confirm = m->selected == 1; // Cancel is initially selected.
        page(m, KP_SETTINGS);
        if (confirm) emit(m, KP_CLEAR_BOND, NULL);
    }
}
static bool bound(const kp_model_t *m, unsigned slot) {
    const kp_binding_t *b = kp_binding(&m->config, slot);
    return b && b->count;
}
static void hold(kp_model_t *m, unsigned key) {
    kp_key_t *k = &m->key[key];
    k->long_sent = true; k->pending = false; k->clicks = 0;
    if (key == 2) { back(m); return; }
    if (m->page == KP_CONSOLE && !m->executing && !k->ignored) {
        m->triggered_slot = key * 4 + 3;
        kp_step_t step;
        if (kp_binding_step(&m->config, kp_binding(&m->config, m->triggered_slot), 0, &step))
            emit(m, KP_HOLD, &step.action);
    }
}
void kp_tick(kp_model_t *m, uint32_t now) {
    kp_screen_tick(&m->screen, &m->screen_config, now,
                   m->page == KP_SCREEN || m->page == KP_SCREEN_EDIT || m->executing);
    if (m->feedback.visible && !m->executing && (int32_t)(now - m->feedback.until) >= 0) m->feedback.visible = false;
    if (m->blocked) return;
    for (unsigned i = 0; i < 3; ++i) {
        kp_key_t *k = &m->key[i];
        unsigned threshold = m->page == KP_CONSOLE && i < 2 ? KP_ACTION_HOLD_MS : KP_SYSTEM_HOLD_MS;
        if (k->down && !k->long_sent && (uint32_t)(now - k->pressed_at) >= threshold) {
            hold(m, i);
            if (i == 2) return;
        }
        if (k->pending && !k->down && (uint32_t)(now - k->released_at) >= KP_CLICK_MS) {
            unsigned clicks = k->clicks;
            k->pending = false; k->clicks = 0;
            if (!k->ignored) click(m, i, clicks);
        }
    }
}
void kp_input(kp_model_t *m, unsigned key, bool down, uint32_t now) {
    if (key >= 3) return;
    if (!kp_screen_input(&m->screen, key, down, now)) {
        // A wake must not become a click, a shortcut hold, or a system back.
        memset(m->key, 0, sizeof(m->key));
        m->down_mask = m->screen.down_mask; m->blocked = m->down_mask != 0;
        kp_tick(m, now);
        return;
    }
    kp_tick(m, now);
    uint8_t bit = 1U << key;
    bool was_down = (m->down_mask & bit) != 0;
    if (down) m->down_mask |= bit; else m->down_mask &= ~bit;
    if (m->blocked) { if (!m->down_mask) m->blocked = false; return; }
    if (was_down == down) return;
    kp_key_t *k = &m->key[key];
    if (down) {
        if (!k->pending) k->clicks = 0;
        k->pending = false; k->down = true; k->long_sent = false; k->pressed_at = now;
        k->ignored = m->page == KP_CONSOLE && m->executing;
        if (m->page == KP_CONSOLE && key < 2 && !k->ignored &&
            !bound(m, key * 4) && !bound(m, key * 4 + 1) &&
            !bound(m, key * 4 + 2) && bound(m, key * 4 + 3)) hold(m, key);
    } else {
        k->down = false;
        if (k->ignored) { memset(k, 0, sizeof(*k)); return; }
        if (k->long_sent) {
            if (m->triggered_slot == key * 4 + 3) emit(m, KP_RELEASE, NULL);
            k->long_sent = false;
        }
        else {
            ++k->clicks;
            unsigned limit = m->page != KP_CONSOLE ? 1 :
                bound(m, key * 4 + 2) ? 3 : bound(m, key * 4 + 1) ? 2 : 1;
            if (k->clicks >= limit) {
                unsigned clicks = k->clicks; k->clicks = 0;
                click(m, key, clicks);
            } else { k->pending = true; k->released_at = now; }
        }
    }
}
