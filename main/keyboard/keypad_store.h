#pragma once
#include "keypad_model.h"
#include "esp_err.h"
esp_err_t kp_store_init(void);
// Copies the active configuration under a mutex. A failed save never replaces it.
void kp_store_get(kp_config_t *out);
// Read the committed configuration without a full-size copy. The callback runs
// under the store mutex; it must not retain the pointer or call store APIs.
typedef bool (*kp_store_reader_t)(const kp_config_t *config, void *user);
bool kp_store_read(kp_store_reader_t reader, void *user);
esp_err_t kp_store_save(const kp_config_t *config);
