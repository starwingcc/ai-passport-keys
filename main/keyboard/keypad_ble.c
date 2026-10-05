#include "keypad_ble.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/ble_att.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include <string.h>

// Standard six-key rollover keyboard + a separate 16-bit Consumer Control report.
static const uint8_t report_map[] = {
    0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x01,
    0x05,0x07,0x19,0xe0,0x29,0xe7,0x15,0x00,0x25,0x01,
    0x75,0x01,0x95,0x08,0x81,0x02,
    0x75,0x08,0x95,0x01,0x81,0x01,
    0x05,0x08,0x19,0x01,0x29,0x05,0x95,0x05,0x75,0x01,0x91,0x02,
    0x95,0x01,0x75,0x03,0x91,0x01,
    0x05,0x07,0x19,0x00,0x29,0x73,0x15,0x00,0x25,0x73,
    0x95,0x06,0x75,0x08,0x81,0x00,0xc0,
    0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x02,
    0x15,0x00,0x26,0xff,0x03,0x19,0x00,0x2a,0xff,0x03,
    0x75,0x10,0x95,0x01,0x81,0x00,0xc0
};
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static kp_ble_status_t s_status;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_input[2], s_boot, s_protocol_handle;
static bool s_notify[2], s_boot_notify, s_encrypted, s_boot_mode;
static uint8_t s_addr_type;
static void (*s_hid_register)(struct ble_gatt_register_ctxt *, void *);
static void (*s_hid_sync)(void);
static void (*s_hid_reset)(int);
static struct ble_npl_event s_job;
static SemaphoreHandle_t s_done;
typedef enum { JOB_SEND, JOB_PAUSE, JOB_RESUME, JOB_FORGET, JOB_RECOVER, JOB_POLL } job_command_t;
static job_command_t s_command;
static kp_action_t s_action;
static esp_err_t s_result;
static bool s_job_pending;
static int gap_event(struct ble_gap_event *event, void *arg);
void ble_store_config_init(void);
// Exported by IDF 5.5.3's ble_gap.c; its public header declares the callback
// type but omits this prototype. Keep this declaration matched to that SDK.
void ble_gap_conn_foreach_handle(int (*cb)(uint16_t, void *), void *arg);

kp_ble_status_t kp_ble_status(void) {
    portENTER_CRITICAL(&s_mux);
    kp_ble_status_t value = s_status;
    portEXIT_CRITICAL(&s_mux);
    return value;
}
static void readiness(void) {
    portENTER_CRITICAL(&s_mux);
    bool old_ready = s_status.ready;
    s_status.secure = s_encrypted;
    s_status.keyboard_subscribed = s_boot_mode ? s_boot_notify : s_notify[0];
    s_status.handles_ready = s_input[0] && s_input[1] && s_boot && s_protocol_handle;
    bool ready = s_status.connected && s_encrypted && !s_status.paused &&
                 s_status.handles_ready && s_status.keyboard_subscribed;
    if (s_status.ready != ready) ++s_status.generation;
    s_status.ready = ready;
    if (ready) s_status.failed = false;
    portEXIT_CRITICAL(&s_mux);
    if (old_ready != ready) ESP_LOGI("keypad_ble", "HID ready=%d encrypted=%d keyboard_notify=%d boot=%d",
                                   ready, s_encrypted, s_status.keyboard_subscribed, s_boot_mode);
}
static int count_connection(uint16_t handle, void *arg) {
    (void)handle;
    ++*(unsigned *)arg;
    return 0;
}
static void advertise(void) {
    kp_ble_status_t status = kp_ble_status();
    if (!status.synced || status.paused || s_conn != BLE_HS_CONN_HANDLE_NONE || ble_gap_adv_active()) return;
    // NimBLE allocates the connection before delivering CONNECT (remote feature
    // exchange completes first). Do not retry connectable advertising in that
    // interval: MAX_CONNECTIONS=1 would report BLE_HS_ENOMEM, despite free heap.
    unsigned connections = 0;
    ble_gap_conn_foreach_handle(count_connection, &connections);
    if (connections) return;
    struct ble_hs_adv_fields fields = {0};
    // IDF's automatic connection retry retains this pointer after we return.
    static ble_uuid16_t service = BLE_UUID16_INIT(0x1812);
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.appearance = 0x03c1; fields.appearance_is_present = 1;
    fields.uuids16 = &service; fields.num_uuids16 = 1; fields.uuids16_is_complete = 1;
    const char *name = "Passport Keys";
    fields.name = (uint8_t *)name; fields.name_len = strlen(name); fields.name_is_complete = 1;
    struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = 48, .itvl_max = 80,
    };
    int rc = ble_gap_adv_set_fields(&fields);
    const char *stage = "fields";
    if (!rc) { stage = "start"; rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL); }
    if (rc && rc != BLE_HS_EALREADY) {
        portENTER_CRITICAL(&s_mux); s_status.failed = true; portEXIT_CRITICAL(&s_mux);
        ESP_LOGE("keypad_ble", "Advertising %s failed: %d", stage, rc);
    } else {
        portENTER_CRITICAL(&s_mux); s_status.failed = false; portEXIT_CRITICAL(&s_mux);
    }
}
static void synced(void) {
    if (s_hid_sync) s_hid_sync();
    int rc = ble_hs_id_infer_auto(0, &s_addr_type);
    portENTER_CRITICAL(&s_mux);
    s_status.synced = rc == 0; s_status.failed = rc != 0;
    portEXIT_CRITICAL(&s_mux);
    ESP_LOGI("keypad_ble", "Sync rc=%d handles keyboard=%u consumer=%u boot=%u protocol=%u",
             rc, s_input[0], s_input[1], s_boot, s_protocol_handle);
    readiness();
    advertise();
}
static void reset(int reason) {
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    s_encrypted = s_notify[0] = s_notify[1] = s_boot_notify = false;
    portENTER_CRITICAL(&s_mux);
    s_status.synced = s_status.connected = s_status.ready = false;
    s_status.secure = s_status.keyboard_subscribed = false;
    ++s_status.generation;
    portEXIT_CRITICAL(&s_mux);
    if (s_hid_reset) s_hid_reset(reason);
}
static void registered(struct ble_gatt_register_ctxt *ctx, void *arg) {
    if (s_hid_register) s_hid_register(ctx, arg);
    if (ctx->op == BLE_GATT_REGISTER_OP_DSC && ble_uuid_u16(ctx->dsc.dsc_def->uuid) == 0x2908) {
        struct os_mbuf *buf = NULL;
        uint8_t ref[2] = {0};
        if (ble_att_svr_read_local(ctx->dsc.handle, &buf) == 0) {
            if (ble_hs_mbuf_to_flat(buf, ref, sizeof(ref), NULL) == 0 &&
                ref[1] == ESP_HID_REPORT_TYPE_INPUT && ref[0] >= 1 && ref[0] <= 2)
                s_input[ref[0] - 1] = *(uint16_t *)ctx->dsc.dsc_def->arg;
            os_mbuf_free_chain(buf);
        }
    }
    if (ctx->op != BLE_GATT_REGISTER_OP_CHR) return;
    unsigned uuid = ble_uuid_u16(ctx->chr.chr_def->uuid);
    if (uuid == 0x2a22) s_boot = ctx->chr.val_handle;
    if (uuid == 0x2a4e) s_protocol_handle = ctx->chr.val_handle;
}
static bool allowed_peer(const struct ble_gap_conn_desc *desc) {
    ble_addr_t peers[1]; int count = 0;
    if (ble_store_util_bonded_peers(peers, &count, 1)) return false;
    return count == 0 || ble_addr_cmp(&peers[0], &desc->peer_id_addr) == 0;
}
static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    struct ble_gap_conn_desc desc;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("keypad_ble", "Connect status=%d handle=%u", event->connect.status, event->connect.conn_handle);
        if (event->connect.status) { advertise(); break; }
        s_conn = event->connect.conn_handle;
        s_notify[0] = s_notify[1] = s_boot_notify = s_encrypted = false;
        s_boot_mode = false;
        portENTER_CRITICAL(&s_mux);
        s_status.connected = true; s_status.ready = s_status.failed = false; ++s_status.generation;
        portEXIT_CRITICAL(&s_mux);
        if (kp_ble_status().paused || ble_gap_conn_find(s_conn, &desc) || !allowed_peer(&desc)) {
            ESP_LOGW("keypad_ble", "Rejecting peer: paused, missing connection, or different bond");
            ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        } else {
            int rc = ble_gap_security_initiate(s_conn);
            ESP_LOGI("keypad_ble", "Security initiate rc=%d", rc);
            if (rc && rc != BLE_HS_EALREADY && rc != BLE_HS_EBUSY) {
                portENTER_CRITICAL(&s_mux); s_status.failed = true; portEXIT_CRITICAL(&s_mux);
                ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            }
        }
        readiness();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("keypad_ble", "Disconnect reason=%d", event->disconnect.reason);
        s_conn = BLE_HS_CONN_HANDLE_NONE; s_encrypted = false;
        s_notify[0] = s_notify[1] = s_boot_notify = false;
        portENTER_CRITICAL(&s_mux);
        s_status.connected = s_status.ready = false; ++s_status.generation;
        portEXIT_CRITICAL(&s_mux);
        readiness();
        advertise();
        break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        int found = ble_gap_conn_find(s_conn, &desc);
        if (found) memset(&desc, 0, sizeof(desc));
        ESP_LOGI("keypad_ble", "Security status=%d lookup=%d encrypted=%d authenticated=%d bonded=%d key_size=%u",
                 event->enc_change.status, found, desc.sec_state.encrypted,
                 desc.sec_state.authenticated, desc.sec_state.bonded, desc.sec_state.key_size);
        s_encrypted = !event->enc_change.status && !found &&
                      desc.sec_state.encrypted && desc.sec_state.bonded &&
                      desc.sec_state.key_size == 16;
        readiness();
        if (!s_encrypted) ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        break;
    }
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGD("keypad_ble", "Subscribe handle=%u notify=%d keyboard=%u consumer=%u boot=%u",
                 event->subscribe.attr_handle, event->subscribe.cur_notify, s_input[0], s_input[1], s_boot);
        for (unsigned i = 0; i < 2; ++i)
            if (event->subscribe.attr_handle == s_input[i]) s_notify[i] = event->subscribe.cur_notify;
        if (event->subscribe.attr_handle == s_boot) s_boot_notify = event->subscribe.cur_notify;
        readiness();
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        // NO_IO pairing must never request passkey interaction. Fail closed
        // instead of retaining an unused numeric-confirmation UI.
        ESP_LOGW("keypad_ble", "Unexpected pairing interaction; reconnect using Just Works");
        s_encrypted = false;
        readiness();
        ble_gap_terminate(event->passkey.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        ESP_LOGW("keypad_ble", "Repeated pairing rejected; clear the old bond in Settings first");
        return BLE_GAP_REPEAT_PAIRING_IGNORE; // Explicitly clear the bond in Settings first.
    case BLE_GAP_EVENT_ADV_COMPLETE: advertise(); break;
    default: break;
    }
    return 0;
}
static esp_err_t notify(uint16_t handle, const uint8_t *data, unsigned size) {
    if (!handle) return ESP_ERR_INVALID_STATE;
    struct os_mbuf *buf = ble_hs_mbuf_from_flat(data, size);
    if (!buf) return ESP_ERR_NO_MEM;
    return ble_gatts_notify_custom(s_conn, handle, buf) == 0 ? ESP_OK : ESP_FAIL;
}
static esp_err_t send_report(const kp_action_t *action) {
    if (action && (!kp_action_valid(action, true) ||
        (action->type == KP_KEYBOARD && action->logical_modifiers))) return ESP_ERR_INVALID_ARG;
    if (!kp_ble_status().ready) return ESP_ERR_INVALID_STATE;
    if (action && action->type == KP_MEDIA && (s_boot_mode || !s_notify[1])) return ESP_ERR_NOT_SUPPORTED;
    uint8_t keyboard[8], consumer[2];
    kp_report(action, keyboard, consumer);
    // Send on the handles registered by esp_hid. This keeps allocation failure
    // recoverable (IDF 5.5.3 input_set asserts on mbuf exhaustion).
    esp_err_t rc = notify(s_boot_mode ? s_boot : s_input[0], keyboard, sizeof(keyboard));
    if (rc == ESP_OK && !s_boot_mode && s_notify[1]) rc = notify(s_input[1], consumer, sizeof(consumer));
    ESP_LOGD("keypad_ble", "Input report type=%u boot=%d result=%d",
             action ? (unsigned)action->type : 0, s_boot_mode, (int)rc);
    return rc;
}
static bool read_byte(uint16_t handle, uint8_t *value) {
    struct os_mbuf *buf = NULL;
    if (!handle || ble_att_svr_read_local(handle, &buf)) return false;
    bool ok = ble_hs_mbuf_to_flat(buf, value, 1, NULL) == 0;
    os_mbuf_free_chain(buf);
    return ok;
}
static void refresh_protocol(void) {
    if (s_conn == BLE_HS_CONN_HANDLE_NONE) return;
    uint8_t protocol = 1;
    if (!read_byte(s_protocol_handle, &protocol)) return;
    bool boot = protocol == ESP_HID_PROTOCOL_MODE_BOOT;
    if (boot != s_boot_mode) {
        s_boot_mode = boot;
        portENTER_CRITICAL(&s_mux); ++s_status.generation; portEXIT_CRITICAL(&s_mux);
        readiness();
    }
}
static void job(struct ble_npl_event *event) {
    (void)event;
    s_result = ESP_OK;
    refresh_protocol();
    switch (s_command) {
    case JOB_SEND: s_result = send_report(&s_action); break;
    case JOB_PAUSE:
        if (kp_ble_status().ready) (void)send_report(NULL);
        portENTER_CRITICAL(&s_mux); s_status.paused = true; portEXIT_CRITICAL(&s_mux);
        readiness();
        if (ble_gap_adv_active()) ble_gap_adv_stop();
        if (s_conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        break;
    case JOB_RESUME:
        portENTER_CRITICAL(&s_mux); s_status.paused = false; portEXIT_CRITICAL(&s_mux);
        advertise(); break;
    case JOB_FORGET:
        if (s_conn != BLE_HS_CONN_HANDLE_NONE) s_result = ESP_ERR_INVALID_STATE;
        else if (ble_store_clear()) s_result = ESP_FAIL;
        break;
    case JOB_RECOVER:
        if (s_conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        else advertise();
        break;
    case JOB_POLL: advertise(); break;
    }
    xSemaphoreGive(s_done);
}
static esp_err_t command(job_command_t code, const kp_action_t *action) {
    if (!s_done || !kp_ble_status().synced) return ESP_ERR_INVALID_STATE;
    // Single caller: the application worker. A timed-out job retains ownership
    // of the shared command until its completion has been consumed.
    if (s_job_pending) {
        if (xSemaphoreTake(s_done, 0) != pdTRUE) return ESP_ERR_TIMEOUT;
        s_job_pending = false;
    }
    s_command = code;
    memset(&s_action, 0, sizeof(s_action));
    if (action) s_action = *action;
    s_job_pending = true;
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_job);
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    s_job_pending = false;
    return s_result;
}
esp_err_t kp_ble_send(const kp_action_t *action) { return command(JOB_SEND, action); }
void kp_ble_recover(void) { (void)command(JOB_RECOVER, NULL); }
void kp_ble_poll(void) { (void)command(JOB_POLL, NULL); }
esp_err_t kp_ble_pause(bool pause) {
    esp_err_t rc = command(pause ? JOB_PAUSE : JOB_RESUME, NULL);
    if (rc != ESP_OK || !pause) return rc;
    for (unsigned i = 0; i < 100; ++i) {
        if (!kp_ble_status().connected) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_ERR_TIMEOUT;
}
esp_err_t kp_ble_forget(void) {
    esp_err_t rc = kp_ble_pause(true);
    if (rc == ESP_OK) rc = command(JOB_FORGET, NULL);
    esp_err_t resume = kp_ble_pause(false);
    return rc == ESP_OK ? resume : rc;
}
static void host_task(void *param) {
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
esp_err_t kp_ble_init(void) {
    s_done = xSemaphoreCreateBinary();
    if (!s_done) return ESP_ERR_NO_MEM;
    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) return rc;
    static esp_hid_raw_report_map_t maps[] = {{.data = report_map, .len = sizeof(report_map)}};
    esp_hid_device_config_t config = {
        .device_name = "Passport Keys", .manufacturer_name = "Passport",
        .serial_number = "1", .report_maps = maps, .report_maps_len = 1,
    };
    esp_hidd_dev_t *hid;
    rc = esp_hidd_dev_init(&config, ESP_HID_TRANSPORT_BLE, NULL, &hid);
    if (rc != ESP_OK) return rc;
    s_hid_sync = ble_hs_cfg.sync_cb;
    s_hid_reset = ble_hs_cfg.reset_cb;
    s_hid_register = ble_hs_cfg.gatts_register_cb;
    ble_hs_cfg.sync_cb = synced;
    ble_hs_cfg.reset_cb = reset;
    ble_hs_cfg.gatts_register_cb = registered;
    // Just Works pairing: no passkey, with encryption and a persistent bond.
    // The authenticated flag denotes MITM protection and is intentionally not
    // required for this user-selected pairing method.
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1; ble_hs_cfg.sm_mitm = 0; ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    ble_svc_gap_device_name_set("Passport Keys");
    ble_npl_event_init(&s_job, job, NULL);
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}
