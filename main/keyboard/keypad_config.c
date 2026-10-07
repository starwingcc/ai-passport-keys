#include "keypad_config.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    kp_json_read_t read; void *user;
    char buffer[256]; size_t pos, size, total, limit;
    bool failed;
} parser_t;
static int peek(parser_t *p) {
    if (p->failed) return -1;
    if (p->pos == p->size) {
        int size = p->read(p->user, p->buffer, sizeof(p->buffer));
        if (size < 0 || size > (int)sizeof(p->buffer)) { p->failed = true; return -1; }
        p->pos = 0; p->size = (size_t)size;
        if (!size) return -1;
    }
    return (unsigned char)p->buffer[p->pos];
}
static int take(parser_t *p) {
    int c = peek(p);
    if (c >= 0) { ++p->pos; if (++p->total > (p->limit ? p->limit : KP_JSON_BYTES)) { p->failed = true; return -1; } }
    return c;
}
static void space(parser_t *p) {
    int c;
    while ((c = peek(p)) == ' ' || c == '\n' || c == '\r' || c == '\t') (void)take(p);
}
static bool token(parser_t *p, int expected) { space(p); return take(p) == expected; }
static int hex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool unicode(parser_t *p, uint32_t *out) {
    *out = 0;
    for (unsigned i = 0; i < 4; ++i) {
        int n = hex(take(p)); if (n < 0) return false;
        *out = (*out << 4) | (unsigned)n;
    }
    return true;
}
static bool string(parser_t *p, char *out, size_t size) {
    if (!token(p, '"')) return false;
    size_t used = 0;
    for (;;) {
        int c = take(p);
        if (c < 0x20) return false;
        if (c == '"') { out[used] = 0; return true; }
        if (c == '\\') {
            c = take(p);
            if (c == 'u') {
                uint32_t cp;
                if (!unicode(p, &cp) || !cp) return false;
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    uint32_t low;
                    if (take(p) != '\\' || take(p) != 'u' || !unicode(p, &low) || low < 0xdc00 || low > 0xdfff) return false;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                } else if (cp >= 0xdc00 && cp <= 0xdfff) return false;
                unsigned char bytes[4]; unsigned count;
                if (cp < 0x80) { bytes[0] = cp; count = 1; }
                else if (cp < 0x800) { bytes[0] = 0xc0 | (cp >> 6); bytes[1] = 0x80 | (cp & 63); count = 2; }
                else if (cp < 0x10000) { bytes[0] = 0xe0 | (cp >> 12); bytes[1] = 0x80 | ((cp >> 6) & 63); bytes[2] = 0x80 | (cp & 63); count = 3; }
                else { bytes[0] = 0xf0 | (cp >> 18); bytes[1] = 0x80 | ((cp >> 12) & 63); bytes[2] = 0x80 | ((cp >> 6) & 63); bytes[3] = 0x80 | (cp & 63); count = 4; }
                if (used + count >= size) return false;
                memcpy(out + used, bytes, count); used += count; continue;
            }
            switch (c) {
            case '"': case '\\': case '/': break;
            case 'b': c = '\b'; break; case 'f': c = '\f'; break; case 'n': c = '\n'; break;
            case 'r': c = '\r'; break; case 't': c = '\t'; break;
            default: return false;
            }
        }
        if (used + 1 >= size) return false;
        out[used++] = (char)c;
    }
}
static bool number(parser_t *p, unsigned max, unsigned *out) {
    space(p); int c = peek(p);
    if (c < '0' || c > '9') return false;
    bool zero = c == '0'; unsigned value = 0, digits = 0;
    while ((c = peek(p)) >= '0' && c <= '9') {
        if ((zero && digits) || value > max / 10 || (value == max / 10 && (unsigned)(c - '0') > max % 10)) return false;
        value = value * 10 + (unsigned)(take(p) - '0'); ++digits;
    }
    *out = value; return true;
}
static bool field(parser_t *p, const char *const *names, unsigned count, unsigned *seen, unsigned *index) {
    char name[32];
    if (!string(p, name, sizeof(name)) || !token(p, ':')) return false;
    for (unsigned i = 0; i < count; ++i) if (!strcmp(name, names[i])) {
        if (*seen & (1U << i)) return false;
        *seen |= 1U << i; *index = i; return true;
    }
    return false;
}
// Returns 1 for another item, 0 for the end, -1 for malformed delimiters.
static int more(parser_t *p, int end) {
    space(p); int c = take(p);
    if (c == end) return 0;
    return c == ',' ? 1 : -1;
}
static bool step(parser_t *p, kp_step_t *out) {
    const char *const names[] = {"type", "modifiers", "keys", "consumer", "delay_ms", "logical_modifiers", "system"};
    unsigned seen = 0, f, value; int next;
    unsigned consumer = 0, logical = 0;
    memset(out, 0, sizeof(*out));
    if (!token(p, '{')) return false;
    do {
        if (!field(p, names, 7, &seen, &f)) return false;
        if (f == 2) {
            if (!token(p, '[')) return false;
            space(p);
            if (peek(p) == ']') { (void)take(p); continue; }
            unsigned count = 0;
            do {
                if (count == 6 || !number(p, 255, &value) || !value) return false;
                out->action.keys[count++] = value;
            } while ((next = more(p, ']')) == 1);
            if (next < 0) return false;
        } else {
            unsigned max = f == 0 ? KP_MEDIA : f == 1 ? 255 : f == 3 ? 65535 : f == 5 ? 3 : f == 6 ? KP_TARGET_COUNT - 1 : KP_MAX_DELAY_MS;
            if (!number(p, max, &value)) return false;
            if (f == 0) out->action.type = value;
            else if (f == 1) out->action.modifiers = value;
            else if (f == 3) consumer = value;
            else if (f == 5) logical = value;
            else if (f == 6) out->action.system = value;
            else out->delay_ms = value;
        }
    } while ((next = more(p, '}')) == 1);
    if (next != 0 || ((seen & 31) != 31) ||
        (out->action.type == KP_KEYBOARD ? consumer != 0 : logical != 0)) return false;
    if (out->action.type == KP_KEYBOARD)
        out->action.modifiers |= (logical & 1 ? 0x08 : 0) | (logical & 2 ? 0x80 : 0);
    else { if (consumer > 255) return false; out->action.consumer = consumer; }
    return kp_action_valid(&out->action, false);
}
static bool binding(parser_t *p, kp_config_t *c, unsigned profile, unsigned slot) {
    const char *const names[] = {"title", "steps"};
    char title[KP_NAME_CHARS * 4 + 1] = "";
    kp_step_t steps[KP_MAX_STEPS]; unsigned count = 0, seen = 0, f; int next;
    if (!token(p, '{')) return false;
    do {
        if (!field(p, names, 2, &seen, &f)) return false;
        if (f == 0) {
            if (!string(p, title, sizeof(title))) return false;
        } else {
            if (!token(p, '[')) return false;
            space(p);
            if (peek(p) == ']') { (void)take(p); continue; }
            do {
                if (count == KP_MAX_STEPS || !step(p, &steps[count++])) return false;
            } while ((next = more(p, ']')) == 1);
            if (next < 0) return false;
        }
    } while ((next = more(p, '}')) == 1);
    return next == 0 && seen == 3 && kp_binding_set(c, profile, slot, title, steps, count);
}
static bool profile(parser_t *p, kp_config_t *c) {
    const char *const names[] = {"name", "bindings"};
    char name[KP_NAME_CHARS * 4 + 1]; unsigned seen = 0, f, index = c->profile_count++; int next;
    if (!token(p, '{')) return false;
    do {
        if (!field(p, names, 2, &seen, &f)) return false;
        if (f == 0) {
            if (!string(p, name, sizeof(name)) || !kp_profile_name_set(c, index, name)) return false;
        } else {
            if (!token(p, '[')) return false;
            for (unsigned i = 0; i < KP_BINDINGS; ++i) {
                if (i && !token(p, ',')) return false;
                if (!binding(p, c, index, i)) return false;
            }
            if (!token(p, ']')) return false;
        }
    } while ((next = more(p, '}')) == 1);
    return next == 0 && seen == 3;
}
static bool config_object(parser_t *p, kp_config_t *out) {
    const char *const names[] = {"version", "active_profile", "profiles"};
    unsigned seen = 0, f, value; int next;
    kp_config_empty(out);
    if (!token(p, '{')) return false;
    do {
        if (!field(p, names, 3, &seen, &f)) return false;
        if (f == 2) {
            if (!token(p, '[')) return false;
            do {
                if (out->profile_count == KP_MAX_PROFILES || !profile(p, out)) return false;
            } while ((next = more(p, ']')) == 1);
            if (next < 0) return false;
        } else {
            if (!number(p, f == 0 ? KP_VERSION : KP_MAX_PROFILES - 1, &value)) return false;
            if (f == 0) out->version = value; else out->active_profile = value;
        }
    } while ((next = more(p, '}')) == 1);
    if (next != 0 || seen != 7 || (out->version != 5 && out->version != KP_VERSION)) return false;
    out->version = KP_VERSION;
    return kp_config_valid(out);
}
bool kp_config_decode(kp_json_read_t read, void *user, kp_config_t *out) {
    if (!read || !out) return false;
    parser_t p = {.read = read, .user = user};
    if (!config_object(&p, out)) return false;
    space(&p); return peek(&p) < 0 && !p.failed;
}
typedef struct { const char *data; size_t used; } memory_t;
static int memory_read(void *user, char *buffer, size_t capacity) {
    memory_t *m = user; size_t count = strlen(m->data + m->used);
    if (count > capacity) count = capacity;
    memcpy(buffer, m->data + m->used, count); m->used += count; return (int)count;
}
bool kp_config_decode_string(const char *json, kp_config_t *out) {
    if (!json) return false;
    memory_t m = {.data = json}; return kp_config_decode(memory_read, &m, out);
}
static bool home_object(parser_t *p, kp_home_config_t *out) {
    const char *const names[] = {"version", "name", "badge", "avatar", "timezone_minutes", "hour12"};
    unsigned seen = 0, f, value; int next;
    kp_home_defaults(out);
    if (!token(p, '{')) return false;
    do {
        if (!field(p, names, 6, &seen, &f)) return false;
        if (f == 1 || f == 2) {
            if (!string(p, f == 1 ? out->name : out->badge, sizeof(out->name))) return false;
        } else {
            space(p); bool negative = f == 4 && peek(p) == '-';
            if (negative) (void)take(p);
            unsigned max = f == 3 ? KP_AVATAR_CHIP : f == 4 ? 840 : 1;
            if (!number(p, max, &value)) return false;
            if (f == 0) out->version = value;
            else if (f == 3) out->avatar = value;
            else if (f == 4) out->timezone_minutes = negative ? -(int)value : (int)value;
            else out->hour12 = value;
        }
    } while ((next = more(p, '}')) == 1);
    if (next != 0 || seen != 63) return false;
    return kp_home_valid(out, false);
}
bool kp_home_decode(kp_json_read_t read, void *user, kp_home_config_t *out) {
    if (!read || !out) return false;
    parser_t p = {.read = read, .user = user};
    if (!home_object(&p, out)) return false;
    space(&p); return peek(&p) < 0 && !p.failed && p.total <= KP_HOME_JSON_BYTES;
}
static bool wifi_object(parser_t *p, kp_wifi_config_t *out) {
    const char *const names[] = {"version", "ssid", "password"};
    unsigned seen = 0, f, value; int next;
    kp_wifi_defaults(out);
    if (!token(p, '{')) return false;
    do {
        if (!field(p, names, 3, &seen, &f)) return false;
        if (!f) {
            if (!number(p, 1, &value)) return false;
            out->version = value;
        } else if (!string(p, f == 1 ? out->ssid : out->password,
                           f == 1 ? sizeof(out->ssid) : sizeof(out->password))) return false;
    } while ((next = more(p, '}')) == 1);
    return next == 0 && seen == 7 && kp_wifi_valid(out);
}
bool kp_settings_decode(kp_json_read_t read, void *user, kp_settings_t *out) {
    if (!read || !out) return false;
    parser_t p = {.read = read, .user = user, .limit = KP_SETTINGS_JSON_BYTES};
    const char *const names[] = {"version", "home", "wifi", "config"};
    unsigned seen = 0, f, value; int next;
    memset(out, 0, sizeof(*out));
    if (!token(&p, '{')) return false;
    do {
        if (!field(&p, names, 4, &seen, &f)) return false;
        if (!f) {
            if (!number(&p, 1, &value)) return false;
            out->version = value;
        } else if (f == 1) {
            if (!home_object(&p, &out->home)) return false;
        } else if (f == 2) {
            if (!wifi_object(&p, &out->wifi)) return false;
        } else if (!config_object(&p, &out->config)) return false;
    } while ((next = more(&p, '}')) == 1);
    space(&p);
    return next == 0 && seen == 15 && peek(&p) < 0 && !p.failed && kp_settings_valid(out, false);
}
bool kp_settings_decode_string(const char *json, kp_settings_t *out) {
    if (!json) return false;
    memory_t m = {.data = json}; return kp_settings_decode(memory_read, &m, out);
}
bool kp_wifi_decode(kp_json_read_t read, void *user, const kp_wifi_config_t *saved, kp_wifi_config_t *out) {
    if (!read || !saved || !out) return false;
    parser_t p = {.read = read, .user = user};
    const char *const names[] = {"ssid", "password", "keep_password"};
    unsigned seen = 0, f; int next; bool keep = false;
    kp_wifi_defaults(out);
    if (!token(&p, '{')) return false;
    do {
        if (!field(&p, names, 3, &seen, &f)) return false;
        if (f < 2) {
            if (!string(&p, f == 0 ? out->ssid : out->password, f == 0 ? sizeof(out->ssid) : sizeof(out->password))) return false;
        } else {
            space(&p);
            if (peek(&p) == 't') { if (take(&p) != 't' || take(&p) != 'r' || take(&p) != 'u' || take(&p) != 'e') return false; keep = true; }
            else if (take(&p) != 'f' || take(&p) != 'a' || take(&p) != 'l' || take(&p) != 's' || take(&p) != 'e') return false;
        }
    } while ((next = more(&p, '}')) == 1);
    if (next != 0 || seen != 7) return false;
    space(&p);
    if (peek(&p) >= 0 || p.failed || p.total > KP_HOME_JSON_BYTES) return false;
    if (keep) {
        if (out->password[0] || !out->ssid[0] || strcmp(out->ssid, saved->ssid)) return false;
        memcpy(out->password, saved->password, sizeof(out->password));
    }
    return kp_wifi_valid(out);
}
bool kp_time_decode(kp_json_read_t read, void *user, uint64_t *epoch) {
    if (!read || !epoch) return false;
    parser_t p = {.read = read, .user = user}; char name[32]; unsigned value;
    if (!token(&p, '{') || !string(&p, name, sizeof(name)) || strcmp(name, "epoch") ||
        !token(&p, ':') || !number(&p, (unsigned)KP_TIME_MAX, &value) ||
        value < KP_TIME_MIN || !token(&p, '}')) return false;
    space(&p);
    if (peek(&p) >= 0 || p.failed || p.total > KP_HOME_JSON_BYTES) return false;
    *epoch = value; return true;
}
typedef struct { kp_json_write_t write; void *user; char buffer[256]; size_t used; bool failed; } writer_t;
static bool flush(writer_t *w) {
    if (w->failed) return false;
    if (w->used && !w->write(w->user, w->buffer, w->used)) { w->failed = true; return false; }
    w->used = 0; return true;
}
static void output(writer_t *w, const char *text) {
    while (*text && !w->failed) {
        if (w->used == sizeof(w->buffer) && !flush(w)) return;
        w->buffer[w->used++] = *text++;
    }
}
static void quoted(writer_t *w, const char *text) {
    output(w, "\"");
    while (*text) {
        char byte[2] = {*text++, 0};
        if (byte[0] == '"' || byte[0] == '\\') output(w, "\\");
        if (byte[0] == '\n') output(w, "\\n"); else output(w, byte);
    }
    output(w, "\"");
}
bool kp_wifi_job_encode(const kp_wifi_job_t *job, kp_json_write_t write, void *user) {
    if (!write || job->count > KP_WIFI_SCAN_RESULTS) return false;
    writer_t w = {.write = write, .user = user}; char number_text[160];
    snprintf(number_text, sizeof(number_text), "{\"id\":%lu,\"kind\":%u,\"state\":%u,\"busy\":%s,\"connected\":%s,\"restore_failed\":%s,\"reason\":%u,\"ssid\":",
             (unsigned long)job->id, job->kind, job->state, job->busy ? "true" : "false", job->connected ? "true" : "false", job->restore_failed ? "true" : "false", job->reason);
    output(&w, number_text); quoted(&w, job->ssid); output(&w, ",\"networks\":[");
    for (unsigned i = 0; i < job->count; ++i) {
        if (i) output(&w, ",");
        output(&w, "{\"ssid\":"); quoted(&w, job->networks[i].ssid);
        snprintf(number_text, sizeof(number_text), ",\"rssi\":%d,\"secured\":%s}", job->networks[i].rssi, job->networks[i].secured ? "true" : "false");
        output(&w, number_text);
    }
    output(&w, "]}"); return flush(&w);
}
bool kp_wifi_encode(const kp_wifi_config_t *wifi, unsigned status, bool clock_valid, kp_json_write_t write, void *user) {
    if (!write || !kp_wifi_valid(wifi)) return false;
    writer_t w = {.write = write, .user = user}; char tail[100];
    output(&w, "{\"ssid\":"); quoted(&w, wifi->ssid);
    snprintf(tail, sizeof(tail), ",\"has_password\":%s,\"state\":%u,\"clock_valid\":%s}",
             wifi->password[0] ? "true" : "false", status, clock_valid ? "true" : "false");
    output(&w, tail); return flush(&w);
}
bool kp_home_encode(const kp_home_config_t *h, kp_json_write_t write, void *user) {
    if (!write || !kp_home_valid(h, true)) return false;
    writer_t w = {.write = write, .user = user}; char tail[100];
    output(&w, "{\"version\":1,\"name\":"); quoted(&w, h->name);
    output(&w, ",\"badge\":"); quoted(&w, h->badge);
    snprintf(tail, sizeof(tail), ",\"avatar\":%u,\"timezone_minutes\":%d,\"hour12\":%u}", h->avatar, h->timezone_minutes, h->hour12);
    output(&w, tail); return flush(&w);
}
bool kp_config_encode(const kp_config_t *c, kp_json_write_t write, void *user) {
    if (!write || !kp_config_valid(c)) return false;
    writer_t w = {.write = write, .user = user}; char number_text[128];
    snprintf(number_text, sizeof(number_text), "{\"version\":%u,\"active_profile\":%u,\"profiles\":[", KP_VERSION, c->active_profile); output(&w, number_text);
    for (unsigned p = 0; p < c->profile_count; ++p) {
        if (p) output(&w, ",");
        output(&w, "{\"name\":"); quoted(&w, kp_text(c, c->profiles[p].name)); output(&w, ",\"bindings\":[");
        for (unsigned i = 0; i < KP_BINDINGS; ++i) {
            const kp_binding_t *b = &c->profiles[p].bindings[i];
            if (i) output(&w, ",");
            output(&w, "{\"title\":"); quoted(&w, kp_text(c, b->title));
            output(&w, ",\"steps\":[");
            for (unsigned j = 0; j < b->count; ++j) {
                kp_step_t s; if (!kp_binding_step(c, b, j, &s)) return false;
                if (j) output(&w, ",");
                snprintf(number_text, sizeof(number_text), "{\"type\":%u,\"modifiers\":%u,\"keys\":[", s.action.type, s.action.modifiers); output(&w, number_text);
                bool comma = false;
                for (unsigned k = 0; k < 6; ++k) if (s.action.keys[k]) {
                    snprintf(number_text, sizeof(number_text), "%s%u", comma ? "," : "", s.action.keys[k]); output(&w, number_text); comma = true;
                }
                snprintf(number_text, sizeof(number_text), "],\"consumer\":%u,\"delay_ms\":%u,\"system\":%u",
                         s.action.type == KP_MEDIA ? s.action.consumer : 0, s.delay_ms, s.action.system); output(&w, number_text);
                output(&w, "}");
            }
            output(&w, "]}");
        }
        output(&w, "]}");
    }
    output(&w, "]}"); return flush(&w);
}
bool kp_settings_encode(const kp_settings_t *s, kp_json_write_t write, void *user) {
    if (!write || !kp_settings_valid(s, true)) return false;
    writer_t w = {.write = write, .user = user};
    output(&w, "{\"version\":1,\"home\":");
    if (!flush(&w) || !kp_home_encode(&s->home, write, user)) return false;
    output(&w, ",\"wifi\":{\"version\":1,\"ssid\":"); quoted(&w, s->wifi.ssid);
    output(&w, ",\"password\":"); quoted(&w, s->wifi.password);
    output(&w, "},\"config\":");
    if (!flush(&w) || !kp_config_encode(&s->config, write, user)) return false;
    output(&w, "}"); return flush(&w);
}
