#include "keypad_store.h"
#include "keypad_migrate.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>

static SemaphoreHandle_t s_lock;
static kp_settings_t s_settings;
static kp_screen_config_t s_screen;
static kp_system_t s_system;
static kp_clock_t s_clock;
static void cleanup_legacy(nvs_handle_t nvs) {
    // Only call after a complete record is durable. Reclaim obsolete blobs so
    // a full shortcut arena still has room for future complete-record updates.
    const char *keys[] = {"config_v1", "config_v2", "config_v3", "config_v4", "config_v5", "home_v1", "wifi_v1"};
    bool changed = false;
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        if (nvs_erase_key(nvs, keys[i]) == ESP_OK) changed = true;
    if (changed) (void)nvs_commit(nvs);
}
static esp_err_t persist(nvs_handle_t nvs, const kp_settings_t *settings) {
    esp_err_t rc = nvs_set_blob(nvs, "settings_v1", settings, kp_settings_size(settings));
    if (rc == ESP_OK) rc = nvs_commit(nvs);
    if (rc == ESP_OK) cleanup_legacy(nvs);
    return rc;
}
esp_err_t kp_store_init(void) {
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    kp_settings_defaults(&s_settings); s_clock = (kp_clock_t){0};
    kp_screen_defaults(&s_screen);
    s_system = KP_SYSTEM_MACOS;
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc != ESP_OK) return rc;
    kp_screen_config_t screen = {0}; size_t screen_size = sizeof(screen);
    if (nvs_get_blob(nvs, "screen_v1", &screen, &screen_size) == ESP_OK &&
        screen_size == sizeof(screen) && kp_screen_valid(&screen)) s_screen = screen;
    uint8_t system[2] = {0}; size_t system_size = sizeof(system);
    if (nvs_get_blob(nvs, "system_v1", system, &system_size) == ESP_OK &&
        system_size == sizeof(system) && system[0] == 1 && system[1] < KP_SYSTEM_COUNT)
        s_system = system[1];
    size_t settings_size = 0;
    rc = nvs_get_blob(nvs, "settings_v1", NULL, &settings_size);
    if (rc == ESP_OK && settings_size >= offsetof(kp_settings_t, config) + offsetof(kp_config_t, data) + 1 &&
        settings_size <= sizeof(kp_settings_t)) {
        kp_settings_t *settings = calloc(1, sizeof(*settings));
        if (!settings) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "settings_v1", settings, &settings_size);
        bool valid = rc == ESP_OK && kp_settings_valid(settings, true) && settings_size == kp_settings_size(settings);
        if (valid) s_settings = *settings;
        free(settings);
        if (valid) { cleanup_legacy(nvs); nvs_close(nvs); return ESP_OK; }
    }
    // Import independent records; reclaim them only after successful migration.
    kp_home_config_t home = {0}; size_t home_size = sizeof(home);
    if (nvs_get_blob(nvs, "home_v1", &home, &home_size) == ESP_OK && home_size == sizeof(home) && kp_home_valid(&home, true)) s_settings.home = home;
    kp_wifi_config_t wifi = {0}; size_t wifi_size = sizeof(wifi);
    if (nvs_get_blob(nvs, "wifi_v1", &wifi, &wifi_size) == ESP_OK && wifi_size == sizeof(wifi) && kp_wifi_valid(&wifi)) s_settings.wifi = wifi;
    size_t size = 0;
    rc = nvs_get_blob(nvs, "config_v5", NULL, &size);
    kp_config_t *saved = NULL;
    if (rc == ESP_OK && size >= offsetof(kp_config_t, data) + 1 && size <= KP_CONFIG_BYTES) {
        saved = calloc(1, sizeof(*saved));
        if (!saved) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "config_v5", saved, &size);
        if (rc == ESP_OK && kp_config_valid(saved) && size == kp_config_size(saved)) {
            s_settings.config = *saved; free(saved);
            (void)persist(nvs, &s_settings); nvs_close(nvs); return ESP_OK;
        }
    }
    free(saved);
    size = 0;
    rc = nvs_get_blob(nvs, "config_v4", NULL, &size);
    if (rc == ESP_OK && size >= offsetof(kp_config_t, data) + 1 && size <= KP_CONFIG_BYTES) {
        saved = calloc(1, sizeof(*saved));
        if (!saved) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "config_v4", saved, &size);
        bool migrated = rc == ESP_OK && kp_migrate_v4(saved, size);
        if (migrated) s_settings.config = *saved;
        free(saved);
        if (migrated) {
            (void)persist(nvs, &s_settings); nvs_close(nvs); return ESP_OK;
        }
    }
    size = 0;
    rc = nvs_get_blob(nvs, "config_v3", NULL, &size);
    if (rc == ESP_OK && size >= offsetof(kp_v3_config_t, data) + 1 && size <= KP_CONFIG_BYTES) {
        kp_v3_config_t *old = calloc(1, sizeof(*old));
        if (!old) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "config_v3", old, &size);
        bool migrated = rc == ESP_OK && kp_migrate_v3(old, size, &s_settings.config);
        free(old);
        if (migrated) {
            // Keep legacy keys; retry a failed migration commit on next boot.
            (void)persist(nvs, &s_settings); nvs_close(nvs); return ESP_OK;
        }
    }
    size = 0;
    rc = nvs_get_blob(nvs, "config_v2", NULL, &size);
    if (rc == ESP_OK && size >= offsetof(kp_v2_config_t, data) + 1 && size <= KP_CONFIG_BYTES) {
        kp_v2_config_t *old = calloc(1, sizeof(*old));
        if (!old) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "config_v2", old, &size);
        bool migrated = rc == ESP_OK && kp_migrate_v2(old, size, &s_settings.config);
        free(old);
        if (migrated) {
            // If committing the complete record fails, preserve config_v2, use the
            // migrated values in RAM, and retry on the next boot without erasing bonds.
            (void)persist(nvs, &s_settings); nvs_close(nvs); return ESP_OK;
        }
        kp_defaults(&s_settings.config);
    }
    kp_legacy_config_t legacy; size = sizeof(legacy);
    rc = nvs_get_blob(nvs, "config_v1", &legacy, &size);
    if (rc == ESP_OK && size == sizeof(legacy)) (void)kp_migrate(&legacy, &s_settings.config);
    // Keep legacy records; retry a failed migration on the next boot.
    (void)persist(nvs, &s_settings);
    nvs_close(nvs);
    return ESP_OK;
}
void kp_store_get(kp_config_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_settings.config; xSemaphoreGive(s_lock);
}
bool kp_store_read(kp_store_reader_t reader, void *user) {
    if (!reader) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool result = reader(&s_settings.config, user);
    xSemaphoreGive(s_lock);
    return result;
}
bool kp_store_settings_read(kp_store_settings_reader_t reader, void *user) {
    if (!reader) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool result = reader(&s_settings, user);
    xSemaphoreGive(s_lock); return result;
}
esp_err_t kp_store_settings_save(const kp_settings_t *settings) {
    if (!kp_settings_valid(settings, false)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc == ESP_OK) { rc = persist(nvs, settings); nvs_close(nvs); }
    if (rc == ESP_OK) s_settings = *settings;
    xSemaphoreGive(s_lock); return rc;
}
// Device-side scenario changes and legacy API writes also update the complete record.
static esp_err_t save_part(const kp_config_t *config, const kp_home_config_t *home, const kp_wifi_config_t *wifi) {
    kp_settings_t *next = malloc(sizeof(*next));
    if (!next) return ESP_ERR_NO_MEM;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *next = s_settings;
    if (config) next->config = *config;
    if (home) next->home = *home;
    if (wifi) next->wifi = *wifi;
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc == ESP_OK) { rc = persist(nvs, next); nvs_close(nvs); }
    if (rc == ESP_OK) s_settings = *next;
    xSemaphoreGive(s_lock); free(next); return rc;
}
esp_err_t kp_store_save(const kp_config_t *config) {
    return kp_config_valid(config) ? save_part(config, NULL, NULL) : ESP_ERR_INVALID_ARG;
}
void kp_store_home_get(kp_home_config_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_settings.home; xSemaphoreGive(s_lock);
}
esp_err_t kp_store_home_save(const kp_home_config_t *home) {
    return kp_home_valid(home, false) ? save_part(NULL, home, NULL) : ESP_ERR_INVALID_ARG;
}
void kp_store_wifi_get(kp_wifi_config_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_settings.wifi; xSemaphoreGive(s_lock);
}
esp_err_t kp_store_wifi_save(const kp_wifi_config_t *wifi) {
    return kp_wifi_valid(wifi) ? save_part(NULL, NULL, wifi) : ESP_ERR_INVALID_ARG;
}
void kp_store_screen_get(kp_screen_config_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_screen; xSemaphoreGive(s_lock);
}
esp_err_t kp_store_screen_save(const kp_screen_config_t *screen) {
    if (!kp_screen_valid(screen)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc == ESP_OK) {
        rc = nvs_set_blob(nvs, "screen_v1", screen, sizeof(*screen));
        if (rc == ESP_OK) rc = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (rc == ESP_OK) s_screen = *screen;
    xSemaphoreGive(s_lock); return rc;
}
bool kp_store_time_sync(uint64_t epoch, uint64_t now) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool result = kp_clock_sync(&s_clock, epoch, now);
    xSemaphoreGive(s_lock); return result;
}
void kp_store_system_get(kp_system_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_system; xSemaphoreGive(s_lock);
}
esp_err_t kp_store_system_save(kp_system_t system) {
    if ((unsigned)system >= KP_SYSTEM_COUNT) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc == ESP_OK) {
        uint8_t record[] = {1, system};
        rc = nvs_set_blob(nvs, "system_v1", record, sizeof(record));
        if (rc == ESP_OK) rc = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (rc == ESP_OK) s_system = system;
    xSemaphoreGive(s_lock); return rc;
}
uint64_t kp_store_time_read(uint64_t now) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint64_t result = kp_clock_read(&s_clock, now);
    xSemaphoreGive(s_lock); return result;
}
