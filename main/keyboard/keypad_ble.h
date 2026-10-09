#pragma once
#include "esp_err.h"
#include "keypad_model.h"
#include "keypad_pairing.h"
typedef struct {
    bool synced, connected, ready, paused, failed;
    bool secure, keyboard_subscribed, handles_ready;
    kp_pairing_hint_t pairing_hint;
    uint32_t generation;
} kp_ble_status_t;
esp_err_t kp_ble_init(void);
// A thread-safe snapshot. ready requires encryption, a bond and HID subscription.
kp_ble_status_t kp_ble_status(void);
// Synchronous commands, called only by the application worker. NULL releases
// all reports. A timeout does not allow a pending job's buffers to be reused.
esp_err_t kp_ble_send(const kp_action_t *action);
esp_err_t kp_ble_pause(bool pause);
esp_err_t kp_ble_forget(void);
void kp_ble_recover(void);
void kp_ble_poll(void);
