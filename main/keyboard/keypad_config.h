#pragma once
#include "keypad_model.h"
#include "cJSON.h"
bool kp_config_parse(const cJSON *json, kp_config_t *out);
cJSON *kp_config_json(const kp_config_t *config);
