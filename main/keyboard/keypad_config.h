#pragma once
#include "keypad_model.h"
#define KP_JSON_BYTES 32768U
// read returns bytes read, 0 at EOF, or -1 on transport failure.
typedef int (*kp_json_read_t)(void *user, char *buffer, size_t capacity);
typedef bool (*kp_json_write_t)(void *user, const char *buffer, size_t size);
// out is a scratch configuration and may be modified on failure. The active
// configuration is replaced only after validation and a successful NVS commit.
bool kp_config_decode(kp_json_read_t read, void *user, kp_config_t *out);
bool kp_config_decode_string(const char *json, kp_config_t *out);
bool kp_config_encode(const kp_config_t *config, kp_json_write_t write, void *user);
