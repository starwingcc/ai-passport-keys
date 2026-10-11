#include "keypad_session.h"
#include <string.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "esp_random.h"
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
#define LOCK() portENTER_CRITICAL(&s_lock)
#define UNLOCK() portEXIT_CRITICAL(&s_lock)
#else
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
#endif

static kp_session_state_t s_runtime = {.generation = 1};

static uint64_t time_ms(const kp_session_state_t *s, uint64_t now) {
    return (s->paused ? s->paused_at : now) - s->offset;
}

static void change(kp_session_state_t *s, kp_session_mode_t mode) {
    s->mode = mode;
    if (!++s->generation) ++s->generation;
    memset(s->pages, 0, sizeof(s->pages));
    s->ready = s->paused = false;
    s->offset = 0;
}

void kp_session_state_init(kp_session_state_t *s) {
    *s = (kp_session_state_t){.generation = 1};
}

void kp_session_state_view(const kp_session_state_t *s, kp_session_view_t *v) {
    *v = (kp_session_view_t){.mode = s->mode, .generation = s->generation, .ready = s->ready};
    for (unsigned i = 0; i < KP_SESSION_PAGES; ++i) if (s->pages[i].id) {
        if (s->pages[i].joined) ++v->online;
        else ++v->joining;
    }
}

void kp_session_state_poll(kp_session_state_t *s, uint64_t now) {
    for (unsigned i = 0; i < KP_SESSION_PAGES; ++i) if (s->pages[i].id) {
        uint64_t elapsed = (s->pages[i].joined ? time_ms(s, now) : now) - s->pages[i].at;
        // HTTP can preempt a worker holding an earlier sampled timestamp.
        // Such a sample is not an expiration; modular wrap still works.
        if (elapsed <= UINT64_MAX / 2 &&
            elapsed >= (s->pages[i].joined ? KP_SESSION_HEARTBEAT_MS : KP_SESSION_START_MS))
            s->pages[i].id = 0;
    }
    kp_session_view_t v;
    kp_session_state_view(s, &v);
    if (s->mode != KP_SESSION_LISTEN && !v.online && !v.joining) change(s, KP_SESSION_LISTEN);
}

uint32_t kp_session_state_open(kp_session_state_t *s, kp_session_mode_t mode, uint64_t now) {
    if (mode != KP_SESSION_TRANSFER && mode != KP_SESSION_CONFIG) return 0;
    kp_session_state_poll(s, now);
    if (s->mode != mode) change(s, mode);
    // A real page takes over the device menu's startup reservation. Keeping
    // that unused reservation would delay cleanup after the page's last leave.
    for (unsigned i = 0; i < KP_SESSION_PAGES; ++i) if (s->pages[i].prepared) s->pages[i].id = 0;
    for (unsigned i = 0; i < KP_SESSION_PAGES; ++i) if (!s->pages[i].id) {
        if (!++s->next_id) ++s->next_id;
        s->pages[i].id = s->next_id;
        s->pages[i].at = now;
        s->pages[i].joined = s->pages[i].prepared = false;
        return s->next_id;
    }
    return 0;
}

bool kp_session_state_join(kp_session_state_t *s, uint32_t generation, uint32_t id, uint64_t now) {
    kp_session_state_poll(s, now);
    if (generation != s->generation || !id) return false;
    for (unsigned i = 0; i < KP_SESSION_PAGES; ++i) if (s->pages[i].id == id && !s->pages[i].joined) {
        s->pages[i].joined = true;
        s->pages[i].at = time_ms(s, now);
        return true;
    }
    return false;
}

bool kp_session_state_ping(kp_session_state_t *s, uint32_t generation, uint32_t id, uint64_t now) {
    kp_session_state_poll(s, now);
    if (generation != s->generation || !id) return false;
    for (unsigned i = 0; i < KP_SESSION_PAGES; ++i) if (s->pages[i].id == id && s->pages[i].joined) {
        s->pages[i].at = time_ms(s, now);
        return true;
    }
    return false;
}

void kp_session_state_leave(kp_session_state_t *s, uint32_t generation, uint32_t id, uint64_t now) {
    if (generation == s->generation && id) for (unsigned i = 0; i < KP_SESSION_PAGES; ++i)
        if (s->pages[i].id == id) s->pages[i].id = 0;
    kp_session_state_poll(s, now);
}

void kp_session_reset(void) {
    uint32_t boot_generation = 1;
#ifdef ESP_PLATFORM
    // Avoid accepting an old tab's generation after a device reboot.
    boot_generation = esp_random();
    if (!boot_generation) boot_generation = 1;
#endif
    LOCK();
    kp_session_state_init(&s_runtime);
    s_runtime.generation = boot_generation;
    UNLOCK();
}

void kp_session_poll(uint64_t now) {
    LOCK();
    kp_session_state_poll(&s_runtime, now);
    UNLOCK();
}

void kp_session_get(kp_session_view_t *out) {
    LOCK();
    kp_session_state_view(&s_runtime, out);
    UNLOCK();
}

uint32_t kp_session_open(kp_session_mode_t mode, uint64_t now, uint32_t *generation) {
    LOCK();
    uint32_t id = kp_session_state_open(&s_runtime, mode, now);
    *generation = s_runtime.generation;
    UNLOCK();
    return id;
}

void kp_session_prepare(kp_session_mode_t mode, uint64_t now) {
    LOCK();
    kp_session_state_poll(&s_runtime, now);
    if (s_runtime.mode != mode) {
        uint32_t id = kp_session_state_open(&s_runtime, mode, now);
        for (unsigned i = 0; id && i < KP_SESSION_PAGES; ++i) if (s_runtime.pages[i].id == id)
            s_runtime.pages[i].prepared = true;
    }
    UNLOCK();
}

uint32_t kp_session_reserve(kp_session_mode_t mode, uint32_t generation, uint64_t now) {
    LOCK();
    kp_session_state_poll(&s_runtime, now);
    uint32_t id = 0;
    if (s_runtime.mode == mode && s_runtime.generation == generation)
        id = kp_session_state_open(&s_runtime, mode, now);
    UNLOCK();
    return id;
}

bool kp_session_current(kp_session_mode_t mode, uint32_t generation) {
    LOCK();
    bool ok = s_runtime.mode == mode && s_runtime.generation == generation;
    UNLOCK();
    return ok;
}

bool kp_session_admitted(kp_session_mode_t mode, uint32_t generation, uint32_t id) {
    LOCK();
    bool ok = false;
    if (s_runtime.mode == mode && s_runtime.generation == generation && id)
        for (unsigned i = 0; i < KP_SESSION_PAGES; ++i) if (s_runtime.pages[i].id == id && !s_runtime.pages[i].joined)
            ok = true;
    UNLOCK();
    return ok;
}

bool kp_session_join(kp_session_mode_t mode, uint32_t generation, uint32_t id, uint64_t now) {
    LOCK();
    bool ok = s_runtime.mode == mode && kp_session_state_join(&s_runtime, generation, id, now);
    UNLOCK();
    return ok;
}

bool kp_session_ping(kp_session_mode_t mode, uint32_t generation, uint32_t id, uint64_t now) {
    LOCK();
    bool ok = s_runtime.mode == mode && kp_session_state_ping(&s_runtime, generation, id, now);
    UNLOCK();
    return ok;
}

void kp_session_leave(kp_session_mode_t mode, uint32_t generation, uint32_t id, uint64_t now) {
    LOCK();
    if (s_runtime.mode == mode) kp_session_state_leave(&s_runtime, generation, id, now);
    UNLOCK();
}

void kp_session_ready(kp_session_mode_t mode, uint32_t generation, bool ready) {
    LOCK();
    if (s_runtime.mode == mode && s_runtime.generation == generation) s_runtime.ready = ready;
    UNLOCK();
}

void kp_session_end(kp_session_mode_t mode) {
    LOCK();
    if (s_runtime.mode == mode) change(&s_runtime, KP_SESSION_LISTEN);
    UNLOCK();
}

void kp_session_pause(uint32_t generation, bool pause, uint64_t now) {
    LOCK();
    if (s_runtime.generation == generation && s_runtime.paused != pause) {
        if (pause) s_runtime.paused_at = now;
        else s_runtime.offset += now - s_runtime.paused_at;
        s_runtime.paused = pause;
    }
    UNLOCK();
}
