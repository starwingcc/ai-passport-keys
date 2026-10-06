#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef void (*kp_web_request_t)(bool forget_bond);
// Start/stop run on the network worker after BLE is paused. request runs on
// the HTTP task and must only enqueue a request; it must not stop its own server.
esp_err_t kp_web_start(kp_web_request_t request);
esp_err_t kp_web_stop(void);
const char *kp_web_ssid(void);
const char *kp_web_password(void);
