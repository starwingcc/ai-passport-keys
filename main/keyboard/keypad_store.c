#include "keypad_store.h"
#include "keypad_migrate.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>

static SemaphoreHandle_t s_lock;
static kp_config_t s_config;
static esp_err_t persist(nvs_handle_t nvs, const kp_config_t *c) {
    esp_err_t rc = nvs_set_blob(nvs, "config_v4", c, kp_config_size(c));
    return rc == ESP_OK ? nvs_commit(nvs) : rc;
}
esp_err_t kp_store_init(void) {
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    kp_defaults(&s_config);
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc != ESP_OK) return rc;
    size_t size = 0;
    rc = nvs_get_blob(nvs, "config_v4", NULL, &size);
    kp_config_t *saved = NULL;
    if (rc == ESP_OK && size >= offsetof(kp_config_t, data) + 1 && size <= KP_CONFIG_BYTES) {
        saved = calloc(1, sizeof(*saved));
        if (!saved) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "config_v4", saved, &size);
        if (rc == ESP_OK && kp_config_valid(saved) && size == kp_config_size(saved)) {
            s_config = *saved; free(saved); nvs_close(nvs); return ESP_OK;
        }
    }
    free(saved);
    size = 0;
    rc = nvs_get_blob(nvs, "config_v3", NULL, &size);
    if (rc == ESP_OK && size >= offsetof(kp_v3_config_t, data) + 1 && size <= KP_CONFIG_BYTES) {
        kp_v3_config_t *old = calloc(1, sizeof(*old));
        if (!old) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "config_v3", old, &size);
        bool migrated = rc == ESP_OK && kp_migrate_v3(old, size, &s_config);
        free(old);
        if (migrated) {
            // Keep legacy keys; retry a failed migration commit on next boot.
            (void)persist(nvs, &s_config); nvs_close(nvs); return ESP_OK;
        }
    }
    size = 0;
    rc = nvs_get_blob(nvs, "config_v2", NULL, &size);
    if (rc == ESP_OK && size >= offsetof(kp_v2_config_t, data) + 1 && size <= KP_CONFIG_BYTES) {
        kp_v2_config_t *old = calloc(1, sizeof(*old));
        if (!old) { nvs_close(nvs); return ESP_ERR_NO_MEM; }
        rc = nvs_get_blob(nvs, "config_v2", old, &size);
        bool migrated = rc == ESP_OK && kp_migrate_v2(old, size, &s_config);
        free(old);
        if (migrated) {
            // Preserve config_v2 even if committing v4 fails. Use the migrated
            // values in RAM and retry on the next boot, without erasing bonds.
            (void)persist(nvs, &s_config); nvs_close(nvs); return ESP_OK;
        }
        kp_defaults(&s_config);
    }
    kp_legacy_config_t legacy; size = sizeof(legacy);
    rc = nvs_get_blob(nvs, "config_v1", &legacy, &size);
    if (rc == ESP_OK && size == sizeof(legacy) && kp_migrate(&legacy, &s_config)) {
        // Keep config_v1. A failed migration commit still uses its mappings in
        // RAM and retries on a later boot; pairing records are never erased.
        (void)persist(nvs, &s_config);
    }
    nvs_close(nvs);
    return ESP_OK;
}
void kp_store_get(kp_config_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_config; xSemaphoreGive(s_lock);
}
bool kp_store_read(kp_store_reader_t reader, void *user) {
    if (!reader) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool result = reader(&s_config, user);
    xSemaphoreGive(s_lock);
    return result;
}
esp_err_t kp_store_save(const kp_config_t *config) {
    if (!kp_config_valid(config)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc == ESP_OK) { rc = persist(nvs, config); nvs_close(nvs); }
    if (rc == ESP_OK) s_config = *config;
    xSemaphoreGive(s_lock); return rc;
}
