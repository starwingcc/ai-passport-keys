#include "keypad_model.h"
#include <stdio.h>
#include <string.h>

static const uint16_t media[] = {0xe9, 0xea, 0xe2, 0xcd, 0xb5, 0xb6};
bool kp_key_valid(unsigned key) {
    return (key >= 4 && key <= 0x73) && key != 0x66;
}
void kp_defaults(kp_config_t *c) {
    memset(c, 0, sizeof(*c));
    c->version = KP_VERSION;
    c->bindings[0] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x29}}; // UP: Esc
    c->bindings[2] = (kp_action_t){.type = KP_KEYBOARD, .modifiers = 0x04}; // UP hold: left Option
    c->bindings[3] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x2a}}; // DOWN: Backspace
    c->bindings[4] = (kp_action_t){.type = KP_KEYBOARD, .modifiers = 0x08, .keys = {0x04}}; // DOWN double: Command+A
    c->bindings[5] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x51}}; // DOWN hold: Down arrow
    c->bindings[6] = (kp_action_t){.type = KP_KEYBOARD, .keys = {0x28}}; // OK: Enter
}
bool kp_config_valid(const kp_config_t *c) {
    if (!c || c->version != KP_VERSION) return false;
    for (unsigned n = 0; n < KP_BINDINGS; ++n) {
        const kp_action_t *a = &c->bindings[n];
        bool any = a->modifiers != 0;
        if (a->type > KP_MEDIA) return false;
        for (unsigned i = 0; i < 6; ++i) {
            if (!a->keys[i]) continue;
            if (a->type != KP_KEYBOARD || !kp_key_valid(a->keys[i])) return false;
            any = true;
            for (unsigned j = 0; j < i; ++j) if (a->keys[j] == a->keys[i]) return false;
        }
        if (a->type == KP_KEYBOARD) {
            if (!any || a->consumer) return false;
        } else if (a->type == KP_MEDIA) {
            bool found = false;
            for (unsigned i = 0; i < sizeof(media) / sizeof(media[0]); ++i)
                if (a->consumer == media[i]) found = true;
            if (!found || any) return false;
        } else if (any || a->consumer) return false;
    }
    return true;
}
void kp_report(const kp_action_t *a, uint8_t keyboard[8], uint8_t consumer[2]) {
    memset(keyboard, 0, 8);
    memset(consumer, 0, 2);
    if (!a) return;
    if (a->type == KP_KEYBOARD) {
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
    if (a->type == KP_NONE) { snprintf(out, size, "关闭"); return; }
    if (a->type == KP_MEDIA) {
        const char *labels[] = {"音量加", "音量减", "静音", "播放暂停", "下一首", "上一首"};
        for (unsigned i = 0; i < 6; ++i) if (media[i] == a->consumer) snprintf(out, size, "%s", labels[i]);
        return;
    }
    const char *mods[] = {"Ctrl", "Shift", "Opt", "Cmd", "RCtrl", "RShift", "ROpt", "RCmd"};
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
static void emit(kp_model_t *m, kp_effect_t e, const kp_action_t *a) {
    if (m->emit) m->emit(m->user, e, a);
}
void kp_cancel(kp_model_t *m) {
    memset(m->key, 0, sizeof(m->key));
    m->blocked = m->down_mask != 0;
    emit(m, KP_RELEASE, NULL);
}
static void page(kp_model_t *m, kp_page_t target) {
    kp_cancel(m);
    m->page = target;
    m->selected = 0;
}
void kp_init(kp_model_t *m, const kp_config_t *c, kp_emit_t cb, void *u) {
    memset(m, 0, sizeof(*m));
    if (kp_config_valid(c)) m->config = *c; else kp_defaults(&m->config);
    m->emit = cb; m->user = u; m->page = KP_CONSOLE;
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
    case KP_FORGET: page(m, KP_SETTINGS); break;
    case KP_MENU: kp_cancel(m); break;
    }
}
static void click(kp_model_t *m, unsigned key, bool doubled) {
    if (m->page == KP_CONSOLE) {
        const kp_action_t *a = &m->config.bindings[key * 3 + (doubled ? 1 : 0)];
        if (a->type != KP_NONE) emit(m, KP_TAP, a);
        return;
    }
    if (m->page == KP_HOTSPOT) {
        // Only long OK exits; short keys cannot accidentally stop configuration.
    } else if (key < 2) m->selected ^= 1U;
    else if (m->page == KP_MENU) page(m, m->selected == 0 ? KP_CONSOLE : KP_SETTINGS);
    else if (m->page == KP_SETTINGS) {
        bool ap = m->selected == 0;
        page(m, ap ? KP_HOTSPOT : KP_FORGET);
        if (ap) emit(m, KP_AP_START, NULL);
    } else if (m->page == KP_FORGET) {
        bool confirm = m->selected == 1; // Cancel is initially selected.
        page(m, KP_SETTINGS);
        if (confirm) emit(m, KP_CLEAR_BOND, NULL);
    }
}
void kp_tick(kp_model_t *m, uint32_t now) {
    if (m->blocked) return;
    for (unsigned i = 0; i < 3; ++i) {
        kp_key_t *k = &m->key[i];
        if (k->down && !k->long_sent && (uint32_t)(now - k->pressed_at) >= KP_HOLD_MS) {
            k->long_sent = true; k->pending = false; k->doubled = false;
            if (i == 2) { back(m); return; }
            if (m->page == KP_CONSOLE) {
                const kp_action_t *a = &m->config.bindings[i * 3 + 2];
                if (a->type != KP_NONE) emit(m, KP_HOLD, a);
            }
        }
        if (k->pending && !k->down && (uint32_t)(now - k->released_at) >= KP_CLICK_MS) {
            k->pending = false;
            click(m, i, false);
        }
    }
}
void kp_input(kp_model_t *m, unsigned key, bool down, uint32_t now) {
    if (key >= 3) return;
    kp_tick(m, now);
    uint8_t bit = 1U << key;
    bool was_down = (m->down_mask & bit) != 0;
    if (down) m->down_mask |= bit; else m->down_mask &= ~bit;
    if (m->blocked) { if (!m->down_mask) m->blocked = false; return; }
    if (was_down == down) return;
    kp_key_t *k = &m->key[key];
    if (down) {
        k->doubled = k->pending;
        k->pending = false; k->down = true; k->long_sent = false; k->pressed_at = now;
    } else {
        k->down = false;
        if (k->long_sent) { emit(m, KP_RELEASE, NULL); k->long_sent = false; }
        else if (m->page != KP_CONSOLE || k->doubled) {
            k->doubled = false;
            click(m, key, m->page == KP_CONSOLE);
        } else { k->pending = true; k->released_at = now; }
    }
}
