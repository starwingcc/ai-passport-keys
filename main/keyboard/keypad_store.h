#pragma once
#include "keypad_model.h"
#include "keypad_home.h"
#include "keypad_wifi.h"
#include "keypad_settings.h"
#include "esp_err.h"
esp_err_t kp_store_init(void);
// The reader borrows the committed record while holding the store mutex;
// it must not retain the pointer or call another store API.
typedef bool (*kp_store_settings_reader_t)(const kp_settings_t *settings, void *user);
bool kp_store_settings_read(kp_store_settings_reader_t reader, void *user);
// Validate and persist all three modules before replacing the in-memory record.
esp_err_t kp_store_settings_save(const kp_settings_t *settings);
// Copies the active configuration under a mutex. A failed save never replaces it.
void kp_store_get(kp_config_t *out);
// Read the committed configuration without a full-size copy. The callback runs
// under the store mutex; it must not retain the pointer or call store APIs.
typedef bool (*kp_store_reader_t)(const kp_config_t *config, void *user);
bool kp_store_read(kp_store_reader_t reader, void *user);
esp_err_t kp_store_save(const kp_config_t *config);
void kp_store_home_get(kp_home_config_t *home);
esp_err_t kp_store_home_save(const kp_home_config_t *home);
void kp_store_wifi_get(kp_wifi_config_t *wifi);
esp_err_t kp_store_wifi_save(const kp_wifi_config_t *wifi);
void kp_store_screen_get(kp_screen_config_t *screen);
esp_err_t kp_store_screen_save(const kp_screen_config_t *screen);
void kp_store_system_get(kp_system_t *system);
esp_err_t kp_store_system_save(kp_system_t system);
bool kp_store_time_sync(uint64_t epoch, uint64_t monotonic_ms);
uint64_t kp_store_time_read(uint64_t monotonic_ms);
