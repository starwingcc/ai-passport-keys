#pragma once
#include <stdbool.h>
#include <stdint.h>

#define KP_WEB_PAGES 4U
#define KP_WEB_HEARTBEAT_MS UINT64_C(120000)
#define KP_WEB_QUIET_MS UINT64_C(60000)

// Logical time stops during bounded configuration operations. HTTP task only;
// independent from TCP keep-alives and ESP-IDF for deterministic host tests.
typedef struct {
    struct { uint32_t id; uint64_t last_ms; } pages[KP_WEB_PAGES];
    uint32_t next_id;
    unsigned online;
    uint64_t zero_since_ms, offset_ms, paused_at_ms;
    bool paused;
} kp_web_presence_t;

void kp_web_presence_init(kp_web_presence_t *s, uint64_t now);
void kp_web_presence_poll(kp_web_presence_t *s, uint64_t now);
uint32_t kp_web_presence_join(kp_web_presence_t *s, uint64_t now);
bool kp_web_presence_ping(kp_web_presence_t *s, uint32_t id, uint64_t now);
void kp_web_presence_leave(kp_web_presence_t *s, uint32_t id, uint64_t now);
void kp_web_presence_pause(kp_web_presence_t *s, bool pause, uint64_t now);
bool kp_web_presence_expired(kp_web_presence_t *s, uint64_t now);
