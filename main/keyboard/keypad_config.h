#pragma once
#include "keypad_model.h"
#include "keypad_home.h"
#include "keypad_wifi.h"
#include "keypad_wifi_setup.h"
#include "keypad_settings.h"
#define KP_JSON_BYTES 32768U
// Add bounded room for badge/network metadata beyond the shortcut JSON budget.
#define KP_SETTINGS_JSON_BYTES (KP_JSON_BYTES + KP_HOME_JSON_BYTES)
// read returns bytes read, 0 at EOF, or -1 on transport failure.
typedef int (*kp_json_read_t)(void *user, char *buffer, size_t capacity);
typedef bool (*kp_json_write_t)(void *user, const char *buffer, size_t size);
// out is a scratch configuration and may be modified on failure. The active
// configuration is replaced only after validation and a successful NVS commit.
bool kp_config_decode(kp_json_read_t read, void *user, kp_config_t *out);
bool kp_config_decode_string(const char *json, kp_config_t *out);
bool kp_config_encode(const kp_config_t *config, kp_json_write_t write, void *user);
bool kp_settings_decode(kp_json_read_t read, void *user, kp_settings_t *out);
bool kp_settings_decode_string(const char *json, kp_settings_t *out);
// Includes Wi-Fi credentials. Serve only to an authenticated setup session.
bool kp_settings_encode(const kp_settings_t *settings, kp_json_write_t write, void *user);
bool kp_home_decode(kp_json_read_t read, void *user, kp_home_config_t *out);
bool kp_home_encode(const kp_home_config_t *home, kp_json_write_t write, void *user);
bool kp_time_decode(kp_json_read_t read, void *user, uint64_t *epoch);
bool kp_wifi_decode(kp_json_read_t read, void *user, const kp_wifi_config_t *saved, kp_wifi_config_t *out);
// The response includes only SSID and whether a password exists, never its value.
bool kp_wifi_encode(const kp_wifi_config_t *wifi, unsigned status, bool clock_valid, kp_json_write_t write, void *user);
bool kp_wifi_job_encode(const kp_wifi_job_t *job, kp_json_write_t write, void *user);
