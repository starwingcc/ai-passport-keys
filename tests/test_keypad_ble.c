#include <assert.h>
#include <stdio.h>
#include "../main/keyboard/keypad_ble.c"
void ble_store_config_init(void) {}
static void event(int type){struct ble_gap_event e={.type=type};gap_event(&e,NULL);}
static void subscribe(unsigned h,bool on){struct ble_gap_event e={.type=BLE_GAP_EVENT_SUBSCRIBE,.subscribe={.attr_handle=h,.cur_notify=on}};gap_event(&e,NULL);}
static void report_handle(uint8_t id, uint8_t type, uint16_t handle) {
    ble_uuid16_t uuid = BLE_UUID16_INIT(0x2908);
    struct fake_dsc dsc = {.uuid=&uuid.u, .arg=&handle};
    fake_descriptor_handle = handle + 1;
    fake_report_ref[0] = id; fake_report_ref[1] = type;
    struct ble_gatt_register_ctxt ctx = {.op=BLE_GATT_REGISTER_OP_DSC,
        .dsc={.dsc_def=&dsc, .handle=fake_descriptor_handle}};
    registered(&ctx, NULL);
}
static void characteristic_handle(uint16_t id, uint16_t handle) {
    ble_uuid16_t uuid = BLE_UUID16_INIT(id);
    struct fake_chr chr = {.uuid=&uuid.u};
    struct ble_gatt_register_ctxt ctx = {.op=BLE_GATT_REGISTER_OP_CHR,
        .chr={.chr_def=&chr, .val_handle=handle}};
    registered(&ctx, NULL);
}
int main(void) {
    assert(kp_ble_init()==ESP_OK);synced();assert(fake_adv);
    assert(ble_hs_cfg.sm_io_cap==BLE_SM_IO_CAP_NO_IO && !ble_hs_cfg.sm_mitm);
    assert(ble_hs_cfg.sm_bonding && ble_hs_cfg.sm_sc);
    // A controller connection precedes the delayed application CONNECT event.
    fake_adv=0;fake_connections=1;fake_adv_error=6;kp_ble_poll();
    assert(!fake_adv&&!kp_ble_status().failed);
    fake_connections=0;kp_ble_poll();assert(kp_ble_status().failed);
    fake_adv_error=0;kp_ble_poll();assert(fake_adv&&!kp_ble_status().failed);
    // Discover report handles through the actual registration callback instead
    // of assigning them directly. Output and unknown report IDs are ignored.
    report_handle(1, ESP_HID_REPORT_TYPE_INPUT, 10);
    report_handle(2, ESP_HID_REPORT_TYPE_INPUT, 20);
    report_handle(1, 2, 50);
    report_handle(3, ESP_HID_REPORT_TYPE_INPUT, 60);
    characteristic_handle(0x2a22, 30);
    characteristic_handle(0x2a4e, 40);
    assert(s_input[0]==10 && s_input[1]==20 && s_boot==30 && s_protocol_handle==40);
    fake_desc.sec_state.encrypted=true;fake_desc.sec_state.authenticated=true;
    fake_desc.sec_state.bonded=true;fake_desc.sec_state.key_size=16;
    kp_action_t key={.type=KP_KEYBOARD,.keys={40}},media={.type=KP_MEDIA,.consumer=233};
    event(BLE_GAP_EVENT_CONNECT);assert(!kp_ble_status().ready);
    assert(kp_ble_send(&key)==ESP_ERR_INVALID_STATE);
    event(BLE_GAP_EVENT_ENC_CHANGE);assert(!kp_ble_status().ready);
    s_status.failed=true;
    subscribe(10,true);assert(kp_ble_status().ready&&!kp_ble_status().failed);
    assert(kp_ble_status().secure&&kp_ble_status().keyboard_subscribed&&kp_ble_status().handles_ready);
    assert(kp_ble_send(&key)==ESP_OK&&fake_last_handle==10&&fake_last[2]==40);
    assert(kp_ble_send(&media)==ESP_ERR_NOT_SUPPORTED);
    subscribe(20,true);assert(kp_ble_send(&media)==ESP_OK&&fake_last_handle==20&&fake_last[0]==233);
    assert(kp_ble_send(NULL)==ESP_OK&&fake_last[0]==0);
    fake_fail_alloc=true;assert(kp_ble_send(&key)==ESP_ERR_NO_MEM);fake_fail_alloc=false;
    // Boot protocol only sends the keyboard report; media is explicitly unavailable.
    fake_protocol=0;subscribe(30,true);kp_ble_poll();
    assert(kp_ble_status().ready&&kp_ble_send(&key)==ESP_OK&&fake_last_handle==30);
    assert(kp_ble_send(&media)==ESP_ERR_NOT_SUPPORTED);
    fake_protocol=1;kp_ble_poll();
    // A timed-out command must not have its arguments overwritten by another command.
    fake_defer=true;assert(kp_ble_send(&key)==ESP_ERR_TIMEOUT);
    assert(kp_ble_send(&media)==ESP_ERR_TIMEOUT&&s_action.keys[0]==40);
    fake_defer=false;fake_pending->fn(fake_pending);
    assert(kp_ble_send(NULL)==ESP_OK);
    // Pause stops input immediately, even when disconnection hasn't completed.
    assert(kp_ble_pause(true)==ESP_ERR_TIMEOUT);
    assert(!kp_ble_status().ready&&kp_ble_status().paused&&fake_terminate_count>0);
    event(BLE_GAP_EVENT_DISCONNECT);assert(!fake_adv);
    assert(kp_ble_pause(false)==ESP_OK&&fake_adv);
    // The old bond is never silently replaced by a different computer.
    fake_bonds=1;fake_peer.val[0]=7;fake_desc.peer_id_addr.val[0]=9;
    unsigned terminated=fake_terminate_count;event(BLE_GAP_EVENT_CONNECT);
    assert(fake_terminate_count==(int)terminated+1);event(BLE_GAP_EVENT_DISCONNECT);
    fake_bonds=0;event(BLE_GAP_EVENT_CONNECT);
    event(BLE_GAP_EVENT_ENC_CHANGE);subscribe(10,true);assert(kp_ble_status().ready);
    // Unexpected passkey interaction fails closed in the Just Works-only app.
    terminated=fake_terminate_count;
    event(BLE_GAP_EVENT_PASSKEY_ACTION);
    assert(!kp_ble_status().ready && fake_terminate_count==(int)terminated+1);
    event(BLE_GAP_EVENT_DISCONNECT);
    fake_fail_clear=true;assert(kp_ble_forget()==ESP_FAIL&&!kp_ble_status().paused);fake_fail_clear=false;
    assert(kp_ble_forget()==ESP_OK&&fake_adv);
    reset(1);assert(!kp_ble_status().synced&&!kp_ble_status().ready);synced();assert(kp_ble_status().synced);
    event(BLE_GAP_EVENT_CONNECT);subscribe(10,true);
    fake_desc.sec_state.authenticated=false;event(BLE_GAP_EVENT_ENC_CHANGE);
    // Just Works has no MITM authentication, but retains encrypted bonding.
    assert(kp_ble_status().ready&&kp_ble_send(&key)==ESP_OK);
    fake_desc.sec_state.encrypted=false;event(BLE_GAP_EVENT_ENC_CHANGE);
    assert(!kp_ble_status().ready&&kp_ble_send(&key)==ESP_ERR_INVALID_STATE);
    fake_desc.sec_state.encrypted=true;fake_desc.sec_state.bonded=false;
    event(BLE_GAP_EVENT_ENC_CHANGE);assert(!kp_ble_status().ready);
    fake_desc.sec_state.bonded=true;fake_desc.sec_state.key_size=7;
    event(BLE_GAP_EVENT_ENC_CHANGE);assert(!kp_ble_status().ready);
    puts("Keypad BLE subscriptions, pairing, pause, allocation and timeout recovery: PASS");
}
