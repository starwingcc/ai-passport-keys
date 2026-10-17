#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    unsigned online;
    uint32_t generation;
    bool allocated;
} kp_transfer_presence_t;

// Worker-only lifecycle, started only for a manually enabled transfer.
// HTTP admission never starts or switches the device function.
esp_err_t kp_transfer_web_start(const char *ip);
// Complete shutdown; failed cleanup retains ownership until a later retry.
esp_err_t kp_transfer_web_stop(void);
void kp_transfer_web_poll(uint64_t now);
void kp_transfer_web_presence(kp_transfer_presence_t *out);
bool kp_transfer_web_running(void);
bool kp_transfer_web_ready(void);
