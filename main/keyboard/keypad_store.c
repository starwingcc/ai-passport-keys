#include "keypad_store.h"
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
static esp_err_t persist(nvs_handle_t nvs, const kp_settings_t *settings) {
    esp_err_t rc = nvs_set_blob(nvs, "settings_v1", settings, kp_settings_size(settings));
    if (rc == ESP_OK) rc = nvs_commit(nvs);
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
        bool migrated = settings->config.version == 5;
        bool valid = rc == ESP_OK && kp_config_upgrade(&settings->config) && kp_settings_valid(settings, true) && settings_size == kp_settings_size(settings);
        if (valid) { s_settings = *settings; if (migrated) (void)persist(nvs, settings); }
        free(settings);
        if (valid) { nvs_close(nvs); return ESP_OK; }
    }
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
// Partial setting updates persist the complete record in one commit.
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
