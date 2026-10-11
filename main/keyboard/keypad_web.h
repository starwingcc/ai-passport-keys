#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef void (*kp_web_request_t)(bool forget_bond);
// Start/stop run on the network worker, which pauses BLE for SoftAP. request runs on
// the HTTP task and must only enqueue a request; it must not stop its own server.
esp_err_t kp_web_start(kp_web_request_t request);
// Reuse the owned STA connection; stop only HTTP, leaving STA with its owner.
esp_err_t kp_web_start_wifi(const char *ip, kp_web_request_t request);
// Rebind after a temporary connection test without losing the setup session.
void kp_web_update_ip(const char *ip);
// Called periodically by the owner; idle HTTP cleanup runs on the HTTP task.
void kp_web_poll(uint32_t now);
esp_err_t kp_web_stop(void);
const char *kp_web_ssid(void);
const char *kp_web_password(void);

// Worker reads the generation owned by the current configuration HTTP service.
uint32_t kp_web_generation(void);
