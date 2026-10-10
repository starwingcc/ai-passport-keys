#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include "keypad_transfer_lifecycle.h"
typedef void (*kp_transfer_wake_t)(uint64_t now_ms);
// Worker-only lifecycle. A single HTTP instance survives catalog release.
esp_err_t kp_transfer_web_listen_start(const char *ip, kp_transfer_wake_t wake);
esp_err_t kp_transfer_web_activate(uint64_t zero_since_ms);
// force ends current browsers; otherwise the HTTP thread rechecks quiet expiry.
esp_err_t kp_transfer_web_deactivate(bool force);
// Invalidate old browsers on IP loss while retaining allocated catalogs.
esp_err_t kp_transfer_web_disconnect(void);
// Complete HTTP shutdown for configuration handoff; retains ownership on failure.
esp_err_t kp_transfer_web_stop(void);
void kp_transfer_web_poll(uint64_t now);
void kp_transfer_web_presence(kp_transfer_presence_t *out);
unsigned kp_transfer_web_peers(void);
bool kp_transfer_web_running(void);
bool kp_transfer_web_ready(void);
bool kp_transfer_web_listening(void);
