#pragma once
#include "keypad_model.h"
#include "keypad_ble.h"
#include "keypad_home.h"
void kp_ui_init(void);
void kp_ui_update(const kp_model_t *model, kp_ble_status_t ble, int battery,
                  const char *message, const char *ssid, const char *password,
                  const kp_home_config_t *home, uint64_t epoch);
