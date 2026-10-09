#pragma once
#include "esp_err.h"
#include "keypad_model.h"
#include "keypad_pairing.h"
typedef struct {
    bool synced, connected, ready, paused, failed;
    bool secure, keyboard_subscribed, handles_ready, release_ready;
    kp_pairing_hint_t pairing_hint;
    uint32_t generation;
    bool peers_valid, peer_valid, input_enabled, pairing;
    uint8_t bond_count, bonds[KP_DEVICE_COUNT][7], peer[7];
    char peer_name[KP_DEVICE_NAME_BYTES];
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

// NULL explicitly opens a free pairing slot. Existing targets are filtered
// using the controller accept list plus the resolved identity at CONNECT.
esp_err_t kp_ble_select(const uint8_t identity[7]);
// Business input is permitted only after the application committed the OS.
// Empty release reports bypass this gate but still require the real HID link.
esp_err_t kp_ble_allow_input(bool allow);
esp_err_t kp_ble_end_pairing(void);
