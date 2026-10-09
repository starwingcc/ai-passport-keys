#include <assert.h>
#include <stdio.h>
#include "../main/keyboard/keypad_ble.c"
#include "../main/keyboard/keypad_runner.c"
#include "../main/keyboard/keypad_devices_runtime.c"
static bool fake_device_save_failure;
static kp_devices_t fake_device_store;
esp_err_t kp_store_devices_save(const kp_devices_t *d) {
    assert(kp_devices_valid(d));
    if (fake_device_save_failure) return ESP_FAIL;
    fake_device_store = *d; return ESP_OK;
}
static int fake_store_result;
static unsigned fake_store_reads;
static int fake_store_read(int type, const union ble_store_key *key, union ble_store_value *value) {
    (void)type; (void)key;
    ++fake_store_reads;
    memset(value, 0, sizeof(*value));
    return fake_store_result;
}
void ble_store_config_init(void) {ble_hs_cfg.store_read_cb = fake_store_read;}
static void event(int type){struct ble_gap_event e={.type=type};e.connect.conn_handle=fake_conn_handle;e.enc_change.conn_handle=fake_conn_handle;e.disconnect.conn.conn_handle=fake_conn_handle;e.passkey.conn_handle=fake_conn_handle;gap_event(&e,NULL);}
static void subscribe(unsigned h,bool on){struct ble_gap_event e={.type=BLE_GAP_EVENT_SUBSCRIBE,.subscribe={.conn_handle=fake_conn_handle,.attr_handle=h,.cur_notify=on}};gap_event(&e,NULL);}
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
static uint8_t captured_keyboard[8];
static unsigned captured_reports;
static void capture_redo(uint16_t handle,const uint8_t *data,size_t size) {
    if(captured_reports++==0){assert(handle==10&&size==8);memcpy(captured_keyboard,data,8);}
    else {assert(captured_reports==2&&handle==20&&size==2&&!data[0]&&!data[1]);}
}
static void redo_branches(void) {
    // Reproduce the reported configuration through the production runner,
    // resolver, BLE worker and HID encoder; only notifications are simulated.
    kp_step_t steps[]={
        {.action={.type=KP_KEYBOARD,.modifiers=0x0a,.keys={29},.system=KP_TARGET_MACOS},.delay_ms=80},
        {.action={.type=KP_KEYBOARD,.modifiers=0x01,.keys={28},.system=KP_TARGET_WINDOWS}}
    };
    kp_config_t config;kp_defaults(&config);
    assert(kp_binding_set(&config,0,10,"重做",steps,2));
    const kp_binding_t *binding=kp_binding(&config,10);
    fake_notify_observer=capture_redo;
    for(unsigned system=0;system<KP_SYSTEM_COUNT;system++){
        kp_runner_t runner={0};kp_action_t action,resolved;unsigned reports=0,presses=0;
        assert(kp_runner_start(&runner,&config,binding,false,system)&&runner.total==1);
        for(unsigned now=0;now<=100;now++)if(kp_runner_request(&runner,now,&action)){
            assert(kp_action_resolve(&action,runner.system,&resolved));
            int before=fake_notify_count;
            captured_reports=0;
            assert(kp_ble_send(resolved.type==KP_NONE?NULL:&resolved)==ESP_OK);
            assert(fake_notify_count==before+2&&captured_reports==2);
            uint8_t expected[8]={0};
            if(resolved.type!=KP_NONE){
                ++presses;expected[0]=system==KP_SYSTEM_MACOS?0x0a:0x01;
                expected[2]=system==KP_SYSTEM_MACOS?29:28;
            }
            assert(!memcmp(captured_keyboard,expected,sizeof(expected)));
            ++reports;kp_runner_result(&runner,true,now);
        }
        assert(!runner.running&&!runner.failed&&runner.completed==1&&reports==2&&presses==1);
        char label[100];kp_binding_label(&config,binding,system,label,sizeof(label));
        assert(!strcmp(label,system==KP_SYSTEM_MACOS?"Shift+Command+Z":"Control+Y"));
        assert(kp_binding_count(&config,binding,system)==runner.total);
    }
    fake_notify_observer=NULL;
    puts("Redo system branches: one matching HID key-down and one release per system: PASS");
}

static void disconnect(void) {event(BLE_GAP_EVENT_DISCONNECT);}
static void connect_peer(ble_addr_t peer, bool early) {
    fake_desc.peer_id_addr=peer;fake_desc.sec_state.encrypted=true;
    fake_desc.sec_state.bonded=true;fake_desc.sec_state.key_size=16;
    if(early){event(BLE_GAP_EVENT_ENC_CHANGE);subscribe(10,true);assert(!kp_ble_status().ready);}
    event(BLE_GAP_EVENT_CONNECT);
    event(BLE_GAP_EVENT_ENC_CHANGE);subscribe(10,true);subscribe(20,true);
}
static void read_name(const char *name, unsigned status, void *arg) {
    struct os_mbuf *buf=ble_hs_mbuf_from_flat(name,strlen(name));
    struct ble_gatt_attr attr={.om=buf};struct ble_gatt_error error={.status=status};
    assert(fake_name_callback);fake_name_callback(fake_conn_handle,&error,&attr,arg);os_mbuf_free_chain(buf);
}
static void worker_reconciliation(void) {
    // Exercise the actual worker and actual BLE job/event handlers together.
    // NVS commit is the only replaced application operation.
    fake_store_result=0;kp_devices_t original;kp_devices_defaults(&original);
    assert(kp_ble_forget()==ESP_OK);
    kp_config_t config;kp_defaults(&config);kp_model_t m;kp_init(&m,&config,NULL,NULL);
    kp_devices_runtime_t runtime={.select_pending=true};m.devices_committed=true;m.pairing_adding=true;
    assert(kp_devices_runtime_sync(&m,&runtime,1)==ESP_OK&&kp_ble_status().pairing);
    fake_desc.peer_id_addr=fake_peer;fake_desc.sec_state.encrypted=true;
    fake_desc.sec_state.bonded=true;fake_desc.sec_state.key_size=16;
    fake_bonds=1;event(BLE_GAP_EVENT_ENC_CHANGE);subscribe(10,true);
    unsigned terminated=fake_terminate_count;
    assert(kp_devices_runtime_sync(&m,&runtime,10)==ESP_OK);
    assert(m.devices.slot[0].occupied&&m.devices.slot[0].system==KP_SYSTEM_UNSET);
    assert(kp_ble_status().pairing&&m.actual_device==-1); // Never recapture the new bond as an excluded old peer.
    event(BLE_GAP_EVENT_CONNECT);subscribe(20,true);
    assert(kp_devices_runtime_sync(&m,&runtime,20)==ESP_OK);
    assert(m.page==KP_SYSTEM&&m.device_selected==0&&!kp_model_can_send(&m));
    assert(fake_terminate_count==(int)terminated&&kp_ble_status().connected); // Adopt new bond without disconnecting it.
    m.system_saving=true;m.selected=KP_SYSTEM_WINDOWS;fake_device_save_failure=true;
    kp_devices_t next=m.devices;next.slot[0].system=m.selected;
    bool saved=kp_store_devices_save(&next)==ESP_OK;kp_system_saved(&m,saved);
    assert(!saved&&!kp_model_can_send(&m));m.page=KP_DEVICE_ACTIONS;
    kp_devices_runtime_sync(&m,&runtime,30);assert(!kp_ble_status().input_enabled);
    // A restart of the pure worker restores the unconfirmed identity, never the initial cursor.
    kp_model_t restart;kp_init(&restart,&config,NULL,NULL);restart.devices=fake_device_store;restart.devices_committed=true;
    kp_devices_runtime_t boot={.select_pending=true};kp_devices_runtime_sync(&restart,&boot,40);
    assert(restart.page==KP_SYSTEM&&!kp_model_can_send(&restart));
    fake_device_save_failure=false;m.page=KP_SYSTEM;m.system_saving=true;
    saved=kp_store_devices_save(&next)==ESP_OK;kp_system_saved(&m,saved);
    m.devices_committed=true;kp_devices_runtime_sync(&m,&runtime,50);assert(kp_model_can_send(&m)&&kp_ble_status().input_enabled&&m.system==KP_SYSTEM_WINDOWS);
    // Configured peers reconnect without opening the picker.
    disconnect();kp_devices_runtime_sync(&m,&runtime,60);connect_peer(fake_peer,false);
    kp_devices_runtime_sync(&m,&runtime,70);assert(m.page==KP_CONSOLE&&kp_model_can_send(&m));
    kp_system_open(&m,0);kp_devices_runtime_sync(&m,&runtime,80);assert(!kp_ble_status().input_enabled);
    m.page=KP_DEVICE_ACTIONS;kp_devices_runtime_sync(&m,&runtime,90);assert(kp_ble_status().input_enabled&&m.system==KP_SYSTEM_WINDOWS);
    // Add B while A's independent system remains committed. Failure to save
    // the new identity blocks all input until persistence succeeds.
    m.pairing_adding=true;runtime.select_pending=true;kp_cancel(&m);
    kp_devices_runtime_sync(&m,&runtime,100);disconnect();kp_devices_runtime_sync(&m,&runtime,110);
    fake_bonds=2;connect_peer(fake_second_peer,false);fake_device_save_failure=true;
    kp_devices_runtime_sync(&m,&runtime,120);
    assert(m.page==KP_SYSTEM&&m.device_selected==1&&!m.devices_committed&&!kp_ble_status().input_enabled);
    assert(m.devices.slot[0].system==KP_SYSTEM_WINDOWS&&m.devices.slot[1].system==KP_SYSTEM_UNSET);
    fake_device_save_failure=false;kp_devices_runtime_sync(&m,&runtime,1200);
    next=m.devices;next.slot[1].system=KP_SYSTEM_MACOS;assert(kp_store_devices_save(&next)==ESP_OK);
    m.system_saving=true;m.selected=KP_SYSTEM_MACOS;kp_system_saved(&m,true);m.devices_committed=true;
    kp_devices_runtime_sync(&m,&runtime,1210);assert(kp_model_can_send(&m)&&m.system==KP_SYSTEM_MACOS);
    for(unsigned i=0;i<6;i++){
        unsigned target=i%2;m.devices.target=target;assert(kp_store_devices_save(&m.devices)==ESP_OK);
        kp_device_switched(&m,true);assert(m.page==KP_CONSOLE);
        runtime.select_pending=true;kp_devices_runtime_sync(&m,&runtime,1300+i*100);
        if(m.actual_device!=(int)target){assert(!kp_model_can_send(&m));disconnect();connect_peer(target?fake_second_peer:fake_peer,false);}
        kp_devices_runtime_sync(&m,&runtime,1310+i*100);
        assert(kp_model_can_send(&m)&&m.page==KP_CONSOLE&&m.system==(target?KP_SYSTEM_MACOS:KP_SYSTEM_WINDOWS));
    }
    // External bond removal synchronously clears name and OS on reconciliation.
    disconnect();fake_bonds=0;kp_ble_poll();kp_devices_runtime_sync(&m,&runtime,2100);
    assert(!kp_devices_count(&m.devices)&&!m.devices.slot[0].name[0]&&m.devices.slot[0].system==KP_SYSTEM_UNSET&&!kp_ble_status().input_enabled);
    puts("Production worker + BLE: early bond ordering, OS failure/back/reboot, A/B persistence, edits, repeated switch and removal: PASS");
}
int main(void) {
    const uint8_t a[7]={0,1,2,3,4,5,6}, b[7]={1,6,5,4,3,2,1};
    fake_peer=(ble_addr_t){.type=a[0]};memcpy(fake_peer.val,a+1,6);
    fake_second_peer=(ble_addr_t){.type=b[0]};memcpy(fake_second_peer.val,b+1,6);
    kp_action_t key={.type=KP_KEYBOARD,.keys={40}},media={.type=KP_MEDIA,.consumer=233};
    assert(kp_ble_init()==ESP_OK);synced();assert(!fake_adv); // No accidental open pairing at boot.
    assert(ble_hs_cfg.sm_bonding&&ble_hs_cfg.sm_sc&&!ble_hs_cfg.sm_mitm);
    report_handle(1,ESP_HID_REPORT_TYPE_INPUT,10);report_handle(2,ESP_HID_REPORT_TYPE_INPUT,20);
    report_handle(1,2,50);report_handle(3,ESP_HID_REPORT_TYPE_INPUT,60);
    characteristic_handle(0x2a22,30);characteristic_handle(0x2a4e,40);
    assert(s_input[0]==10&&s_input[1]==20&&s_boot==30&&s_protocol_handle==40);
    assert(kp_ble_select(NULL)==ESP_OK&&fake_adv&&fake_filter==0);
    fake_adv=0;fake_connections=1;fake_adv_error=6;kp_ble_poll();assert(!kp_ble_status().failed);
    fake_connections=0;kp_ble_poll();assert(kp_ble_status().failed);
    fake_adv_error=0;kp_ble_poll();assert(fake_adv);
    connect_peer(fake_peer,true);assert(kp_ble_status().ready&&!kp_ble_status().input_enabled);
    assert(kp_ble_send(&key)==ESP_ERR_INVALID_STATE&&kp_ble_send(&media)==ESP_ERR_INVALID_STATE);
    assert(kp_ble_send(NULL)==ESP_OK); // Mandatory releases bypass unconfirmed OS.
    fake_bonds=1;kp_ble_poll();assert(kp_ble_status().bond_count==1);
    assert(kp_ble_select(a)==ESP_OK);disconnect();connect_peer(fake_peer,false);
    assert(!kp_ble_status().input_enabled);assert(kp_ble_allow_input(true)==ESP_OK);
    assert(kp_ble_send(&key)==ESP_OK&&fake_last[2]==40);
    assert(kp_ble_send(&media)==ESP_OK&&fake_last_handle==20&&fake_last[0]==233);
    redo_branches();
    // Losing the keyboard CCCD must not prevent a held consumer key release.
    subscribe(10,false);assert(!kp_ble_status().ready&&kp_ble_status().release_ready);
    unsigned partial_reports=fake_notify_count;
    assert(kp_ble_send(NULL)==ESP_OK&&fake_notify_count==(int)partial_reports+1&&fake_last_handle==20&&!fake_last[0]);
    assert(kp_ble_send(&media)==ESP_ERR_INVALID_STATE);subscribe(10,true);
    unsigned terminated=fake_terminate_count;
    assert(kp_ble_select(a)==ESP_OK&&fake_terminate_count==(int)terminated&&kp_ble_status().connected);
    assert(!kp_ble_status().input_enabled);kp_ble_allow_input(true);
    // Editing pauses business input and still transmits an empty release.
    unsigned reports=fake_notify_count;assert(kp_ble_allow_input(false)==ESP_OK);
    assert(fake_notify_count==(int)reports+2&&!fake_last[0]);
    assert(kp_ble_send(&key)==ESP_ERR_INVALID_STATE&&kp_ble_send(NULL)==ESP_OK);
    kp_ble_allow_input(true);fake_fail_alloc=true;assert(kp_ble_send(&key)==ESP_ERR_NO_MEM);fake_fail_alloc=false;
    fake_protocol=0;subscribe(30,true);kp_ble_poll();assert(kp_ble_send(&key)==ESP_OK&&fake_last_handle==30);
    assert(kp_ble_send(&media)==ESP_ERR_NOT_SUPPORTED);fake_protocol=1;kp_ble_poll();
    fake_defer=true;assert(kp_ble_send(&key)==ESP_ERR_TIMEOUT);
    assert(kp_ble_select(b)==ESP_ERR_TIMEOUT&&s_action.keys[0]==40); // Pending job arguments stay owned.
    fake_defer=false;fake_pending->fn(fake_pending);assert(kp_ble_send(NULL)==ESP_OK);
    // Switching releases to A, closes its input gate, then disconnects. B
    // cannot receive old queued input; even a wrong peer with early encryption
    // and restored CCCDs is rejected before the business-input gate opens.
    reports=fake_notify_count;terminated=fake_terminate_count;
    assert(kp_ble_select(b)==ESP_OK&&fake_notify_count==(int)reports+2&&fake_terminate_count==(int)terminated+1);
    assert(!kp_ble_status().ready&&kp_ble_send(&key)==ESP_ERR_INVALID_STATE);disconnect();
    assert(fake_filter==BLE_HCI_ADV_FILT_CONN&&fake_whitelist_count==1&&!ble_addr_cmp(&fake_whitelist,&fake_second_peer));
    terminated=fake_terminate_count;connect_peer(fake_peer,true);assert(fake_terminate_count>(int)terminated&&!kp_ble_status().ready);disconnect();
    fake_bonds=2;connect_peer(fake_second_peer,true);assert(kp_ble_status().ready&&!kp_ble_status().input_enabled);
    kp_ble_allow_input(true);assert(kp_ble_send(&key)==ESP_OK);
    struct ble_gap_event stale={.type=BLE_GAP_EVENT_ENC_CHANGE,.enc_change={.conn_handle=9}};gap_event(&stale,NULL);
    stale=(struct ble_gap_event){.type=BLE_GAP_EVENT_SUBSCRIBE,.subscribe={.conn_handle=9,.attr_handle=10,.cur_notify=false}};gap_event(&stale,NULL);
    stale=(struct ble_gap_event){.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=9}}};gap_event(&stale,NULL);
    assert(kp_ble_status().ready&&kp_ble_status().input_enabled);
    // Name errors, empty/unsupported names and callbacks from a previous
    // connection retain a supported fallback. No name infers an OS.
    read_name("",0,fake_name_arg);assert(!*kp_ble_status().peer_name);
    read_name("Host",5,fake_name_arg);assert(!*kp_ble_status().peer_name);
    read_name("😀",0,fake_name_arg);assert(!*kp_ble_status().peer_name);
    read_name("Windows PC",0,fake_name_arg);assert(!strcmp(kp_ble_status().peer_name,"Windows PC"));
    void *old_arg=fake_name_arg;disconnect();connect_peer(fake_second_peer,false);
    read_name("Wrong Host",0,old_arg);assert(!*kp_ble_status().peer_name);
    assert(kp_ble_select(NULL)==ESP_ERR_INVALID_STATE&&fake_bonds==2); // No automatic overwrite.
    struct ble_store_status_event overflow={.event_code=BLE_STORE_EVENT_OVERFLOW};
    assert(ble_hs_cfg.store_status_cb(&overflow,NULL)==BLE_HS_ENOMEM&&fake_bonds==2);
    // Resolved identity is stable even when the radio RPA changes. The fake
    // connection exposes only NimBLE's peer_id_addr, exactly as production.
    disconnect();connect_peer(fake_second_peer,false);kp_ble_allow_input(true);assert(kp_ble_send(&key)==ESP_OK);
    fake_desc.sec_state.encrypted=false;event(BLE_GAP_EVENT_ENC_CHANGE);assert(!kp_ble_status().ready&&kp_ble_send(&key)==ESP_ERR_INVALID_STATE);
    disconnect();connect_peer(fake_second_peer,false);kp_ble_allow_input(true);
    fake_desc.sec_state.bonded=false;event(BLE_GAP_EVENT_ENC_CHANGE);assert(!kp_ble_status().ready);
    disconnect();fake_desc.sec_state.bonded=true;fake_desc.sec_state.key_size=7;event(BLE_GAP_EVENT_CONNECT);event(BLE_GAP_EVENT_ENC_CHANGE);assert(!kp_ble_status().ready);disconnect();
    assert(kp_ble_pause(true)==ESP_OK&&!fake_adv);assert(kp_ble_pause(false)==ESP_OK&&fake_adv);
    fake_fail_clear=true;assert(kp_ble_forget()==ESP_FAIL&&!kp_ble_status().paused);fake_fail_clear=false;
    assert(kp_ble_forget()==ESP_OK&&fake_bonds==0&&!fake_adv&&kp_ble_status().bond_count==0);
    assert(kp_ble_select(NULL)==ESP_OK&&fake_adv);assert(kp_ble_end_pairing()==ESP_OK&&!fake_adv);
    // Missing-key diagnosis remains available before CONNECT, while normal
    // enumeration, a store error and a peer-key probe do not set the notice.
    union ble_store_key missing={.sec={.peer_addr={.val={1,2,3,4,5,6}}}};union ble_store_value value;
    fake_store_result=BLE_HS_ENOENT;read_bond(BLE_STORE_OBJ_TYPE_PEER_SEC,&missing,&value);
    assert(kp_ble_status().pairing_hint==KP_PAIRING_NONE);
    read_bond(BLE_STORE_OBJ_TYPE_OUR_SEC,&missing,&value);assert(kp_ble_status().pairing_hint==KP_PAIRING_KEY_MISSING);
    assert(kp_ble_forget()==ESP_OK&&kp_ble_status().pairing_hint==KP_PAIRING_NONE);
    reset(1);assert(!kp_ble_status().synced&&!kp_ble_status().ready);synced();
    worker_reconciliation();
    puts("Two-peer BLE identity filters, first confirmation gate, releases, switch, names, no eviction and timeout recovery: PASS");
}
