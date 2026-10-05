#pragma once
#include <stdbool.h>
#include <stdint.h>

#define KP_SCREEN_VERSION 1U
#define KP_SCREEN_FADE_MS 300U
#define KP_SCREEN_TIME_STEP_S 10U
#define KP_SCREEN_DIM_MAX_S 180U
#define KP_SCREEN_OFF_MAX_S 300U
typedef struct {
    uint32_t version;
    uint16_t dim_after_s, off_after_s; // Zero disables that stage independently.
    uint8_t brightness, dim_brightness, reserved[2];
} kp_screen_config_t;
_Static_assert(sizeof(kp_screen_config_t) == 12, "screen settings disk layout");
typedef enum { KP_SCREEN_BRIGHTNESS, KP_SCREEN_DIM_AFTER, KP_SCREEN_DIM_BRIGHTNESS,
               KP_SCREEN_OFF_AFTER, KP_SCREEN_SETTING_COUNT } kp_screen_setting_t;
typedef enum { KP_SCREEN_ACTIVE, KP_SCREEN_DIM, KP_SCREEN_OFF } kp_screen_state_t;
typedef struct {
    uint32_t last_activity, fade_started;
    uint8_t down_mask, level, fade_from;
    bool waking;
    kp_screen_state_t state;
} kp_screen_t;

void kp_screen_defaults(kp_screen_config_t *config);
bool kp_screen_valid(const kp_screen_config_t *config);
unsigned kp_screen_value(const kp_screen_config_t *config, kp_screen_setting_t setting);
// Brightness is bounded; time options wrap and skip incompatible thresholds.
bool kp_screen_adjust(kp_screen_config_t *config, kp_screen_setting_t setting, int direction);
void kp_screen_init(kp_screen_t *screen, const kp_screen_config_t *config, uint32_t now);
// False consumes the entire wake gesture, including held keys and releases.
bool kp_screen_input(kp_screen_t *screen, unsigned key, bool down, uint32_t now);
void kp_screen_tick(kp_screen_t *screen, const kp_screen_config_t *config, uint32_t now, bool inhibit);
