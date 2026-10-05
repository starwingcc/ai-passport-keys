#pragma once
#include "keypad_wifi_setup.h"
#include "esp_err.h"

// Called by the application worker while the configuration AP owns AP+STA.
// Stop must succeed before destroying its netifs or resuming automatic time.
esp_err_t kp_wifi_setup_start(void);
esp_err_t kp_wifi_setup_stop(void);
void kp_wifi_setup_poll(uint32_t now_ms);
// HTTP task APIs only copy/queue data under a mutex; no radio or storage work.
esp_err_t kp_wifi_setup_scan(uint32_t *id);
esp_err_t kp_wifi_setup_test(const kp_wifi_config_t *candidate, uint32_t *id);
void kp_wifi_setup_get(kp_wifi_job_t *out);
