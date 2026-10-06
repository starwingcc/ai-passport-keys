#pragma once
#include <stdbool.h>
#include <stdint.h>

#define KP_HOME_CHARS 16
#define KP_HOME_JSON_BYTES 1024U
#define KP_TIME_MIN 1577836800ULL /* 2020-01-01 UTC */
#define KP_TIME_MAX 4102444799ULL /* 2099-12-31 UTC */
typedef enum { KP_AVATAR_ROBOT, KP_AVATAR_CAT, KP_AVATAR_ASTRO, KP_AVATAR_CHIP } kp_avatar_t;
// Home metadata within the complete configuration; shortcut arena stays 6 KiB.
typedef struct {
    uint32_t version;
    char name[KP_HOME_CHARS * 4 + 1], badge[KP_HOME_CHARS * 4 + 1];
    int16_t timezone_minutes; /* fixed UTC offset, no automatic DST */
    uint8_t avatar, hour12;
} kp_home_config_t;
typedef struct { uint64_t epoch, monotonic_ms; } kp_clock_t;
void kp_home_defaults(kp_home_config_t *home);
bool kp_home_valid(const kp_home_config_t *home, bool stored);
bool kp_clock_sync(kp_clock_t *clock, uint64_t epoch, uint64_t monotonic_ms);
uint64_t kp_clock_read(const kp_clock_t *clock, uint64_t monotonic_ms);
// Output always fits the home card. epoch=0 means no trusted time this boot.
void kp_home_clock_text(const kp_home_config_t *home, uint64_t epoch,
                        char time[6], char date[15], char period[3]);
