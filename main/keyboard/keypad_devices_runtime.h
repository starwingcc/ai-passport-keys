#pragma once
#include "keypad_model.h"
#include "esp_err.h"
typedef struct {
    bool select_pending, prompt_due;
    uint32_t metadata_retry;
} kp_devices_runtime_t;
// Application-worker only: persistence, stable identity reconciliation and
// BLE permission changes. Callback context never executes this work.
esp_err_t kp_devices_runtime_sync(kp_model_t *model, kp_devices_runtime_t *runtime, uint32_t now);
