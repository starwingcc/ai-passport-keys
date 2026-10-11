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
enum { KP_STORE_CONFIG_CHANGED = 1U, KP_STORE_HOME_CHANGED = 2U };
// Main-task synchronization: copy one committed revision without waiting for
// web readers or allocating another settings record. Start revision at zero;
// a busy store leaves it unchanged so the caller can retry on its next tick.
unsigned kp_store_runtime_sync(uint32_t *revision, kp_config_t *config, kp_home_config_t *home);
// Copies the active configuration under a mutex. A failed save never replaces it.
void kp_store_get(kp_config_t *out);
// Read the committed configuration without a full-size copy. The callback runs
// under the store mutex; it must not retain the pointer or call store APIs.
typedef bool (*kp_store_reader_t)(const kp_config_t *config, void *user);
bool kp_store_read(kp_store_reader_t reader, void *user);
esp_err_t kp_store_save(const kp_config_t *config);
// Change only the active profile of the latest committed record.
esp_err_t kp_store_profile_select(unsigned profile);
void kp_store_home_get(kp_home_config_t *home);
esp_err_t kp_store_home_save(const kp_home_config_t *home);
void kp_store_wifi_get(kp_wifi_config_t *wifi);
esp_err_t kp_store_wifi_save(const kp_wifi_config_t *wifi);
void kp_store_screen_get(kp_screen_config_t *screen);
esp_err_t kp_store_screen_save(const kp_screen_config_t *screen);
void kp_store_devices_get(kp_devices_t *devices);
esp_err_t kp_store_devices_save(const kp_devices_t *devices);
void kp_store_system_get(kp_system_t *system);
esp_err_t kp_store_system_save(kp_system_t system);
bool kp_store_time_sync(uint64_t epoch, uint64_t monotonic_ms);
uint64_t kp_store_time_read(uint64_t monotonic_ms);
