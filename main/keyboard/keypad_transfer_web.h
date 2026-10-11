#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include "keypad_transfer_lifecycle.h"
// Worker-only lifecycle. A single HTTP instance survives catalog release.
// The worker observes keypad_session for explicit browser admission/switching.
esp_err_t kp_transfer_web_listen_start(const char *ip);
esp_err_t kp_transfer_web_activate(uint64_t zero_since_ms);
// force ends current browsers; otherwise HTTP rechecks online/admission counts.
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
