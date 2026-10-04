#include "keypad_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <string.h>

static SemaphoreHandle_t s_lock;
static kp_config_t s_config;
esp_err_t kp_store_init(void) {
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    kp_defaults(&s_config);
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc != ESP_OK) return rc;
    kp_config_t saved; size_t size = sizeof(saved);
    rc = nvs_get_blob(nvs, "config_v1", &saved, &size);
    if (rc == ESP_OK && size == sizeof(saved) && kp_config_valid(&saved)) s_config = saved;
    nvs_close(nvs);
    return ESP_OK; // Missing/corrupt configuration uses safe defaults; no whole-NVS erase.
}
void kp_store_get(kp_config_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_config;
    xSemaphoreGive(s_lock);
}
esp_err_t kp_store_save(const kp_config_t *config) {
    if (!kp_config_valid(config)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t nvs;
    esp_err_t rc = nvs_open("passport_keys", NVS_READWRITE, &nvs);
    if (rc == ESP_OK) {
        rc = nvs_set_blob(nvs, "config_v1", config, sizeof(*config));
        if (rc == ESP_OK) rc = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (rc == ESP_OK) s_config = *config;
    xSemaphoreGive(s_lock);
    return rc;
}
