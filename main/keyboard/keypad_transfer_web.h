#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
esp_err_t kp_transfer_web_start(const char *ip);
// Sends end first, allows browser RTC teardown, then stops HTTP. Worker only.
esp_err_t kp_transfer_web_stop(void);
void kp_transfer_web_poll(uint32_t now);
unsigned kp_transfer_web_peers(void);
bool kp_transfer_web_running(void);
bool kp_transfer_web_ready(void);
