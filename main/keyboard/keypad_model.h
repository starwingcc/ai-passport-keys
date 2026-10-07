#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "keypad_screen.h"
#include "keypad_transfer.h"
#include "keypad_lan.h"

#define KP_BINDINGS 11
#define KP_VERSION 5
#define KP_MAX_PROFILES 8
#define KP_MAX_STEPS 64
#define KP_CONFIG_BYTES 6144
#define KP_NAME_CHARS 16
#define KP_CLICK_MS 180U
#define KP_ACTION_HOLD_MS 200U
#define KP_SYSTEM_HOLD_MS 500U
#define KP_PULSE_MS 40U
#define KP_FEEDBACK_MS 500U
#define KP_DELAY_MS 80U
#define KP_MAX_DELAY_MS 5000U

typedef enum { KP_NONE, KP_KEYBOARD, KP_MEDIA } kp_action_type_t;
#define KP_CMD_CTRL_LEFT 1U
#define KP_CMD_CTRL_RIGHT 2U
typedef enum { KP_SYSTEM_MACOS, KP_SYSTEM_WINDOWS, KP_SYSTEM_COUNT } kp_system_t;
// Keyboard logical flags share the type-specific payload with media usage,
// never the eight standard HID modifier bits. Each persisted step uses 12 bytes.
typedef struct {
    uint8_t type, modifiers, keys[6];
    union { uint16_t consumer; uint16_t logical_modifiers; };
} kp_action_t;
typedef struct { kp_action_t action; uint16_t delay_ms; } kp_step_t;
// Offsets refer to data, never to pointers or native heap allocations.
// One descriptor uses a little-endian 32-bit word on ESP32-C3.
// Titles use byte offsets; steps use two-byte units (all step arrays are aligned).
typedef struct { uint32_t title : 13, steps : 12, count : 7; } kp_binding_t;
typedef struct { uint16_t name; kp_binding_t bindings[KP_BINDINGS]; } kp_profile_t;
#define KP_CONFIG_HEADER (8 + KP_MAX_PROFILES * sizeof(kp_profile_t))
typedef struct {
    uint32_t version;
    uint16_t used;
    uint8_t profile_count, active_profile;
    kp_profile_t profiles[KP_MAX_PROFILES];
    uint8_t data[KP_CONFIG_BYTES - KP_CONFIG_HEADER];
} kp_config_t;
_Static_assert(sizeof(kp_config_t) == KP_CONFIG_BYTES, "configuration budget");
_Static_assert(sizeof(kp_binding_t) == 4, "binding layout");
_Static_assert(offsetof(kp_config_t, data) == 392, "configuration header");
_Static_assert(sizeof(kp_step_t) == 12, "persisted step layout");

typedef enum { KP_CONSOLE, KP_MENU, KP_SETTINGS, KP_HOTSPOT, KP_FORGET, KP_PROFILES,
               KP_SCREEN, KP_SCREEN_EDIT, KP_SYSTEM, KP_TRANSFER, KP_LAN } kp_page_t;
typedef enum { KP_TAP, KP_HOLD, KP_RELEASE, KP_AP_START, KP_AP_STOP,
               KP_CLEAR_BOND, KP_CANCEL, KP_SELECT_PROFILE, KP_SAVE_SCREEN, KP_SAVE_SYSTEM, KP_TRANSFER_START, KP_TRANSFER_STOP, KP_WIFI_START } kp_effect_t;
typedef void (*kp_emit_t)(void *user, kp_effect_t effect, const kp_action_t *action);
typedef struct {
    bool down, pending, long_sent, ignored;
    uint8_t clicks;
    uint32_t pressed_at, released_at;
} kp_key_t;
typedef enum { KP_FEEDBACK_RUNNING, KP_FEEDBACK_DONE, KP_FEEDBACK_FAILED } kp_feedback_state_t;
typedef struct {
    bool visible, holding;
    uint8_t slot, completed, total;
    kp_feedback_state_t state;
    uint32_t until, serial;
} kp_feedback_t;
typedef struct {
    kp_config_t config;
    uint32_t config_revision;
    kp_page_t page;
    unsigned selected, triggered_slot;
    kp_key_t key[3];
    uint8_t down_mask;
    bool blocked, executing;
    kp_feedback_t feedback;
    kp_screen_config_t screen_config, screen_draft;
    kp_screen_t screen;
    kp_screen_setting_t screen_setting;
    bool screen_saving, screen_save_failed;
    kp_transfer_view_t transfer;
    kp_lan_view_t lan;
    kp_system_t system;
    bool system_saving, system_save_failed, system_saved;
    kp_emit_t emit;
    void *user;
} kp_model_t;

void kp_config_empty(kp_config_t *config);
bool kp_profile_add(kp_config_t *config, const char *name);
bool kp_profile_name_set(kp_config_t *config, unsigned profile, const char *name);
bool kp_binding_set(kp_config_t *config, unsigned profile, unsigned slot,
                    const char *title, const kp_step_t *steps, unsigned count);
const char *kp_text(const kp_config_t *config, unsigned offset);
const kp_binding_t *kp_binding(const kp_config_t *config, unsigned slot);
bool kp_binding_step(const kp_config_t *config, const kp_binding_t *binding, unsigned index, kp_step_t *out);
size_t kp_config_size(const kp_config_t *config);
bool kp_text_valid(const char *text, unsigned max_chars, bool multiline);
// Stored names survive a font inventory change; new input still uses kp_text_valid.
bool kp_text_utf8_valid(const char *text, unsigned max_chars, bool multiline);
bool kp_codepoint_supported(uint32_t codepoint);
bool kp_action_valid(const kp_action_t *action, bool allow_none);
bool kp_action_resolve(const kp_action_t *action, kp_system_t system, kp_action_t *out);
void kp_defaults(kp_config_t *config);
bool kp_key_valid(unsigned key);
bool kp_config_valid(const kp_config_t *config);
void kp_report(const kp_action_t *action, uint8_t keyboard[8], uint8_t consumer[2]);
void kp_action_label(const kp_action_t *action, char *out, size_t size);
void kp_binding_label(const kp_config_t *config, const kp_binding_t *binding, char *out, size_t size);
void kp_binding_title(const kp_config_t *config, const kp_binding_t *binding, char *out, size_t size);
void kp_init(kp_model_t *model, const kp_config_t *config, kp_emit_t emit, void *user);
void kp_input(kp_model_t *model, unsigned key, bool down, uint32_t now);
void kp_tick(kp_model_t *model, uint32_t now);
void kp_cancel(kp_model_t *model);
void kp_link_changed(kp_model_t *model, uint32_t now);
void kp_set_busy(kp_model_t *model, bool busy);
void kp_hotspot_exit(kp_model_t *model);
void kp_screen_saved(kp_model_t *model, bool success);
void kp_system_saved(kp_model_t *model, bool success);
uint8_t kp_screen_level(const kp_model_t *model);
