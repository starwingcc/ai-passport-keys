#pragma once
#include "keypad_model.h"
#include "esp_err.h"
esp_err_t kp_store_init(void);
// Copies the active configuration under a mutex. A failed save never replaces it.
void kp_store_get(kp_config_t *out);
esp_err_t kp_store_save(const kp_config_t *config);
