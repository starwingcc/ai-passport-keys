#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KP_BINDINGS 8
#define KP_VERSION 1
#define KP_CLICK_MS 180U
#define KP_HOLD_MS 500U

typedef enum { KP_NONE, KP_KEYBOARD, KP_MEDIA } kp_action_type_t;
typedef struct {
    uint8_t type, modifiers, keys[6];
    uint16_t consumer;
} kp_action_t;
// Persisted version-1 layout: UP single/double/hold, DOWN single/double/hold,
// OK single/double. OK hold is reserved and has no binding slot.
typedef struct { uint32_t version; kp_action_t bindings[KP_BINDINGS]; } kp_config_t;
typedef enum { KP_CONSOLE, KP_MENU, KP_SETTINGS, KP_HOTSPOT, KP_FORGET } kp_page_t;
typedef enum { KP_TAP, KP_HOLD, KP_RELEASE, KP_AP_START, KP_AP_STOP,
               KP_CLEAR_BOND } kp_effect_t;
typedef void (*kp_emit_t)(void *user, kp_effect_t effect, const kp_action_t *action);
typedef struct {
    bool down, pending, doubled, long_sent;
    uint32_t pressed_at, released_at;
} kp_key_t;
typedef struct {
    kp_config_t config;
    kp_page_t page;
    unsigned selected;
    kp_key_t key[3];
    uint8_t down_mask;
    bool blocked;
    kp_emit_t emit;
    void *user;
} kp_model_t;

void kp_defaults(kp_config_t *config);
bool kp_key_valid(unsigned key);
bool kp_config_valid(const kp_config_t *config);
void kp_report(const kp_action_t *action, uint8_t keyboard[8], uint8_t consumer[2]);
void kp_action_label(const kp_action_t *action, char *out, size_t size);
void kp_init(kp_model_t *model, const kp_config_t *config, kp_emit_t emit, void *user);
// Deliver ordered physical edges using a wrapping millisecond clock. The caller
// rejects stale queue events and synchronizes down_mask before kp_cancel().
void kp_input(kp_model_t *model, unsigned key, bool down, uint32_t now);
void kp_tick(kp_model_t *model, uint32_t now);
void kp_cancel(kp_model_t *model);
void kp_hotspot_exit(kp_model_t *model);
