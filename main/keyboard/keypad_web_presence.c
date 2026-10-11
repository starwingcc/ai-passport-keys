#include "keypad_web_presence.h"

static uint64_t clock_ms(const kp_web_presence_t *s, uint64_t now) {
    return (s->paused ? s->paused_at_ms : now) - s->offset_ms;
}
void kp_web_presence_init(kp_web_presence_t *s, uint64_t now) {
    *s = (kp_web_presence_t){.zero_since_ms = now};
}
void kp_web_presence_poll(kp_web_presence_t *s, uint64_t now) {
    uint64_t time = clock_ms(s, now), lag = UINT64_MAX;
    unsigned online = 0;
    for (unsigned i = 0; i < KP_WEB_PAGES; ++i) {
        if (!s->pages[i].id) continue;
        uint64_t elapsed = time - s->pages[i].last_ms;
        if (elapsed >= KP_WEB_HEARTBEAT_MS) {
            elapsed -= KP_WEB_HEARTBEAT_MS;
            if (elapsed < lag) lag = elapsed;
            s->pages[i].id = 0;
        } else ++online;
    }
    // Anchor at the actual last lease expiration, even if polling was late.
    if (s->online && !online) s->zero_since_ms = lag == UINT64_MAX ? time : time - lag;
    s->online = online;
}
uint32_t kp_web_presence_join_id(kp_web_presence_t *s, uint32_t id, uint64_t now) {
    kp_web_presence_poll(s, now);
    if (!id) return 0;
    for (unsigned i = 0; i < KP_WEB_PAGES; ++i) if (s->pages[i].id == id) return 0;
    for (unsigned i = 0; i < KP_WEB_PAGES; ++i) if (!s->pages[i].id) {
        s->pages[i].id = id;
        s->pages[i].last_ms = clock_ms(s, now);
        ++s->online;
        return id;
    }
    return 0;
}
uint32_t kp_web_presence_join(kp_web_presence_t *s, uint64_t now) {
    if (!++s->next_id) ++s->next_id;
    return kp_web_presence_join_id(s, s->next_id, now);
}
bool kp_web_presence_ping(kp_web_presence_t *s, uint32_t id, uint64_t now) {
    kp_web_presence_poll(s, now);
    for (unsigned i = 0; i < KP_WEB_PAGES; ++i) if (id && s->pages[i].id == id) {
        s->pages[i].last_ms = clock_ms(s, now);
        return true;
    }
    return false;
}
void kp_web_presence_leave(kp_web_presence_t *s, uint32_t id, uint64_t now) {
    kp_web_presence_poll(s, now);
    for (unsigned i = 0; i < KP_WEB_PAGES; ++i) if (id && s->pages[i].id == id) {
        s->pages[i].id = 0;
        if (!--s->online) s->zero_since_ms = clock_ms(s, now);
        return;
    }
}
void kp_web_presence_pause(kp_web_presence_t *s, bool pause, uint64_t now) {
    if (pause == s->paused) return;
    if (pause) s->paused_at_ms = now;
    else s->offset_ms += now - s->paused_at_ms;
    s->paused = pause;
}
bool kp_web_presence_expired(kp_web_presence_t *s, uint64_t now) {
    kp_web_presence_poll(s, now);
    return !s->paused && !s->online;
}
