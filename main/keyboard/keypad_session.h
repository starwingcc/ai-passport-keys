#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KP_SESSION_PAGES 4U
#define KP_SESSION_START_MS UINT64_C(30000)
#define KP_SESSION_HEARTBEAT_MS UINT64_C(120000)

typedef enum { KP_SESSION_OFF, KP_SESSION_TRANSFER, KP_SESSION_CONFIG } kp_session_mode_t;
typedef struct {
    kp_session_mode_t mode;
    uint32_t generation, next_id;
    struct {
        uint32_t id;
        uint64_t at;
        bool joined;
    } pages[KP_SESSION_PAGES];
    bool ready, paused;
    uint64_t paused_at, offset;
} kp_session_state_t;
typedef struct {
    kp_session_mode_t mode;
    uint32_t generation;
    unsigned online, joining;
    bool ready;
} kp_session_view_t;

// Pure state machine; all timestamps are unsigned monotonic milliseconds.
void kp_session_state_init(kp_session_state_t *s);
void kp_session_state_select(kp_session_state_t *s, kp_session_mode_t mode);
void kp_session_state_poll(kp_session_state_t *s, uint64_t now);
uint32_t kp_session_state_open(kp_session_state_t *s, kp_session_mode_t mode, uint64_t now);
bool kp_session_state_join(kp_session_state_t *s, uint32_t generation, uint32_t id, uint64_t now);
bool kp_session_state_ping(kp_session_state_t *s, uint32_t generation, uint32_t id, uint64_t now);
void kp_session_state_leave(kp_session_state_t *s, uint32_t generation, uint32_t id, uint64_t now);
void kp_session_state_view(const kp_session_state_t *s, kp_session_view_t *v);

// Shared runtime snapshot. Firmware wrappers hold a short critical section;
// no HTTP, storage, or teardown runs under that lock.
void kp_session_reset(void);
void kp_session_poll(uint64_t now);
void kp_session_get(kp_session_view_t *out);
uint32_t kp_session_open(kp_session_mode_t mode, uint64_t now, uint32_t *generation);
// Worker-only manual entry. Stays enabled until explicitly ended; no page lease.
void kp_session_prepare(kp_session_mode_t mode, uint64_t now);
// Automatic retries can reserve only in their original mode and generation.
uint32_t kp_session_reserve(kp_session_mode_t mode, uint32_t generation, uint64_t now);
bool kp_session_join(kp_session_mode_t mode, uint32_t generation, uint32_t id, uint64_t now);
bool kp_session_ping(kp_session_mode_t mode, uint32_t generation, uint32_t id, uint64_t now);
void kp_session_leave(kp_session_mode_t mode, uint32_t generation, uint32_t id, uint64_t now);
bool kp_session_current(kp_session_mode_t mode, uint32_t generation);
void kp_session_ready(kp_session_mode_t mode, uint32_t generation, bool ready);
void kp_session_end(kp_session_mode_t mode);
void kp_session_pause(uint32_t generation, bool pause, uint64_t now);
