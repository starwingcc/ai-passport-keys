#include "keypad_devices_runtime.h"
#include "keypad_ble.h"
#include "keypad_store.h"
#include <string.h>

esp_err_t kp_devices_runtime_sync(kp_model_t *m, kp_devices_runtime_t *runtime, uint32_t now) {
    kp_ble_status_t ble = kp_ble_status();
    esp_err_t result = ESP_OK;
    if (!ble.synced || !ble.peers_valid) { m->hid_ready = false; return ESP_ERR_INVALID_STATE; }
    if (runtime->select_pending && !ble.pairing && ble.bond_count && !ble.connected)
        m->pairing_adding = false;
    kp_devices_t next = m->devices;
    int added = kp_devices_reconcile(&next, ble.bonds, ble.bond_count);
    int actual = ble.connected && ble.peer_valid ? kp_devices_find(&next, ble.peer) : -1;
    if (actual >= 0 && ble.secure && *ble.peer_name)
        (void)kp_device_name_set(&next.slot[actual], ble.peer_name);
    if (added >= 0 && actual == added && ble.secure && m->pairing_adding) {
        next.target = (unsigned)added;
        m->pairing_adding = false; runtime->select_pending = true;
    }
    bool changed = memcmp(&next, &m->devices, sizeof(next)) != 0;
    if (changed) {
        bool target_changed = next.target != m->devices.target;
        m->devices = next; m->devices_committed = false;
        ++m->config_revision;
        runtime->select_pending |= target_changed && !m->pairing_adding;
        if (added >= 0 && actual == added && ble.secure) kp_system_open(m, added);
    }
    // If ENC_CHANGE preceded CONNECT, open once the newly unconfigured peer's
    // stable identity becomes available. Returning voluntarily must not reopen.
    if (m->pairing_adding && ble.pairing && !runtime->select_pending && actual >= 0 && ble.secure) {
        m->pairing_adding = false; m->devices.target = actual;
        m->devices_committed = false; runtime->select_pending = true;
        kp_system_open(m, actual);
    }
    if (!m->devices_committed && (!runtime->metadata_retry || (uint32_t)(now - runtime->metadata_retry) >= 1000)) {
        m->devices_committed = kp_store_devices_save(&m->devices) == ESP_OK;
        runtime->metadata_retry = now;
        if (!m->devices_committed) result = ESP_FAIL;
    }
    if (actual != m->actual_device) runtime->prompt_due = actual >= 0;
    if (runtime->prompt_due && ble.secure && actual >= 0) {
        runtime->prompt_due = false;
        if (m->devices.slot[actual].system == KP_SYSTEM_UNSET) kp_system_open(m, actual);
    }
    m->actual_device = actual;
    m->hid_ready = ble.ready && ble.peer_valid;
    uint8_t target = m->devices.target;
    m->system = target < KP_DEVICE_COUNT ? m->devices.slot[target].system : KP_SYSTEM_UNSET;
    if (runtime->select_pending) {
        const uint8_t *identity = m->pairing_adding || target >= KP_DEVICE_COUNT ? NULL : m->devices.slot[target].identity;
        esp_err_t rc = !identity && !m->pairing_adding ? kp_ble_end_pairing() : kp_ble_select(identity);
        if (rc == ESP_OK) runtime->select_pending = false;
        ble = kp_ble_status();
        m->hid_ready = ble.ready;
    }
    bool allow = !runtime->select_pending && kp_model_can_send(m);
    if (allow != ble.input_enabled) {
        esp_err_t rc = kp_ble_allow_input(allow);
        if (rc != ESP_OK) result = rc;
    }
    return result;
}
