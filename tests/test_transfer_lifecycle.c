#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fake_web.h"
static void quiet_log(const char *tag,const char *format,...){(void)tag;(void)format;}
#undef ESP_LOGI
#define ESP_LOGI(...) quiet_log(__VA_ARGS__)
unsigned esp_get_free_heap_size(void){return 0;}
unsigned esp_get_minimum_free_heap_size(void){return 0;}
size_t heap_caps_get_free_size(unsigned caps){(void)caps;return 0;}
size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 0;}
#include "../main/keyboard/keypad_network_service.c"
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task){(void)task;return 0;}
static bool has_wifi,connected,leased,suspended,web_owned,web_ready,web_up,ap_owned,ble_paused;
static unsigned web_starts,web_stops,ble_pauses;
static esp_err_t web_start_fault,web_stop_fault,suspend_fault,ap_stop_fault,config_start_fault,ble_pause_fault,ble_resume_fault,ap_start_fault;
static unsigned config_polls;
static kp_wifi_job_t config_job;
static kp_transfer_presence_t presence;

static uint64_t test_now;
static uint32_t server_generation,config_generation;
static unsigned suspends;
static char current_ip[16]="192.168.1.11";
static kp_web_request_t request_callback;
int64_t esp_timer_get_time(void){return (int64_t)test_now*1000;}
void vTaskDelay(TickType_t ms){(void)ms;}
int xTaskCreate(void (*f)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *out){assert(f&&name&&stack==4096&&!arg&&priority==3);*out=(void *)1;return pdPASS;}
void kp_store_wifi_get(kp_wifi_config_t *out){kp_wifi_defaults(out);if(has_wifi)strcpy(out->ssid,"saved");}
void kp_network_keep_online(bool keep){leased=keep;}
esp_err_t kp_network_suspend(void){++suspends;if(suspend_fault)return suspend_fault;assert(!web_owned&&!ap_owned);suspended=true;connected=false;return ESP_OK;}
void kp_network_resume(void){suspended=false;}
void kp_network_tick(uint64_t now){(void)now;assert((!ap_owned||leased)&&!suspended);}
bool kp_network_ip(char out[16]){if(connected)strcpy(out,current_ip);else out[0]=0;return connected;}
kp_network_status_t kp_network_status(void){return connected?KP_NET_SYNCED:KP_NET_CONNECTING;}
esp_err_t kp_transfer_web_start(const char *ip){
    assert(connected&&leased&&!ap_owned&&!ble_paused&&!strcmp(ip,current_ip));
    kp_session_view_t current;kp_session_get(&current);server_generation=current.generation;
    if(!web_owned)++web_starts;
    web_owned=web_up=true;
    if(web_start_fault)return web_start_fault;
    presence.allocated=true;web_ready=true;return ESP_OK;
}
esp_err_t kp_transfer_web_stop(void){++web_stops;if(web_stop_fault)return web_stop_fault;web_owned=web_ready=web_up=false;presence.allocated=false;presence.online=0;return ESP_OK;}
bool kp_transfer_web_running(void){return web_owned;}
bool kp_transfer_web_ready(void){return web_ready&&web_up&&kp_session_current(KP_SESSION_TRANSFER,server_generation);}
void kp_transfer_web_presence(kp_transfer_presence_t *out){*out=presence;}
void kp_transfer_web_poll(uint64_t now){(void)now;assert(web_owned);}
esp_err_t kp_ble_pause(bool pause){++ble_pauses;if(!pause){assert(!ap_owned);if(ble_resume_fault)return ble_resume_fault;}ble_paused=pause;return pause?ble_pause_fault:ESP_OK;}
esp_err_t kp_web_start(kp_web_request_t cb){assert(suspended&&!web_owned&&ble_paused);if(ap_start_fault)return ap_start_fault;ap_owned=true;request_callback=cb;return ESP_OK;}
esp_err_t kp_web_stop(void){if(ap_stop_fault)return ap_stop_fault;ap_owned=false;return ESP_OK;}
void kp_web_poll(uint32_t now){(void)now;assert(ap_owned);++config_polls;}
const char *kp_web_ssid(void){return "Passport-Keys-test";}
const char *kp_web_password(void){return "test-pass";}
uint32_t kp_web_generation(void){return config_generation;}
esp_err_t kp_web_start_wifi(const char *ip,kp_web_request_t cb){kp_session_view_t current;kp_session_get(&current);config_generation=current.generation;assert(connected&&leased&&!suspended&&!ap_owned&&!web_owned&&!ble_paused&&!strcmp(ip,current_ip));if(config_start_fault)return config_start_fault;ap_owned=true;request_callback=cb;return ESP_OK;}
bool kp_wifi_setup_testing(void){return config_job.busy&&config_job.kind==KP_WIFI_TEST;}
void kp_web_update_ip(const char *ip){assert(ap_owned&&!strcmp(ip,current_ip));}
void kp_wifi_setup_poll(uint32_t now){(void)now;assert(ap_owned);}
static void tick(service_t *s,uint64_t now){test_now=now;kp_session_poll(now);kp_session_view_t current;kp_session_get(&current);presence.online=current.mode==KP_SESSION_TRANSFER?current.online:0;step(s,now);}
static uint32_t open_mode(kp_session_mode_t mode,uint64_t now,uint32_t *generation){test_now=now;uint32_t lease=kp_session_open(mode,now,generation);assert(lease);return lease;}
int main(void){
    service_t s={0};assert(kp_network_service_start()==ESP_OK);tick(&s,0);assert(!leased&&!web_owned);
    has_wifi=true;connected=true;tick(&s,10);assert(!web_owned&&!leased);
    uint32_t g;assert(!kp_session_open(KP_SESSION_TRANSFER,20,&g));tick(&s,20);assert(!web_owned);
    kp_network_service_transfer(true);tick(&s,30);assert(web_ready&&leased&&s.view.transfer.state==KP_TRANSFER_WAITING);
    tick(&s,30030);tick(&s,120000);assert(presence.allocated&&web_owned&&leased);
    uint32_t a=open_mode(KP_SESSION_TRANSFER,130000,&g);assert(kp_session_join(KP_SESSION_TRANSFER,g,a,130001));tick(&s,130001);
    uint32_t b=open_mode(KP_SESSION_TRANSFER,130002,&g);assert(kp_session_join(KP_SESSION_TRANSFER,g,b,130003));tick(&s,130003);
    kp_session_leave(KP_SESSION_TRANSFER,g,a,130004);tick(&s,130004);assert(s.view.transfer.peers==1);
    kp_session_leave(KP_SESSION_TRANSFER,g,b,130005);tick(&s,130005);assert(presence.allocated&&web_owned&&leased);
    a=open_mode(KP_SESSION_TRANSFER,140000,&g);assert(kp_session_join(KP_SESSION_TRANSFER,g,a,140001));
    tick(&s,260001);assert(web_ready&&leased&&!s.view.transfer.peers);
    assert(!kp_session_open(KP_SESSION_CONFIG,260002,&g));tick(&s,260002);assert(web_owned&&!ap_owned);
    // A manual switch relinquishes HTTP, while keeping the shared STA and BLE.
    unsigned radio_stops=suspends,ble_calls=ble_pauses;
    kp_network_service_config(true,true);web_stop_fault=ESP_FAIL;tick(&s,270000);
    assert(web_owned&&!ap_owned&&leased&&s.view.transfer.state==KP_TRANSFER_STOPPING);
    web_stop_fault=0;tick(&s,271000);assert(ap_owned&&!web_owned&&leased&&connected&&suspends==radio_stops&&ble_pauses==ble_calls);
    uint32_t cg,c=open_mode(KP_SESSION_CONFIG,271001,&cg);assert(kp_session_join(KP_SESSION_CONFIG,cg,c,271002));tick(&s,271002);
    kp_session_leave(KP_SESSION_CONFIG,cg,c,271003);tick(&s,271003);assert(ap_owned&&leased);
    tick(&s,500000);assert(ap_owned&&leased);assert(!kp_session_open(KP_SESSION_TRANSFER,500001,&g));
    // Exit requests stop the full server; no automatic transfer listener returns.
    request_callback(false);ap_stop_fault=ESP_FAIL;tick(&s,500002);assert(ap_owned&&leased&&!web_owned);
    ap_stop_fault=0;tick(&s,500052);assert(!ap_owned&&!web_owned&&!leased&&connected);
    tick(&s,500102);assert(!web_owned&&!leased);
    kp_network_service_transfer(true);web_start_fault=ESP_FAIL;tick(&s,510000);assert(web_owned&&!web_ready&&leased);
    web_start_fault=0;tick(&s,511000);assert(web_ready&&leased);
    // IP loss retains manual intent, and incomplete cleanup blocks re-creation.
    connected=false;web_stop_fault=ESP_FAIL;tick(&s,512000);assert(web_owned&&leased&&!s.view.transfer.ip[0]);
    connected=true;web_stop_fault=0;tick(&s,513000);assert(web_ready&&leased);
    kp_network_service_transfer(false);web_stop_fault=ESP_FAIL;tick(&s,514000);assert(web_owned&&leased);
    web_stop_fault=0;tick(&s,515000);assert(!web_owned&&!leased&&!s.view.transfer.enabled);
    // SoftAP retains explicit exit and BLE ownership.
    kp_network_service_ap(true);tick(&s,520000);assert(ap_owned&&ble_paused&&!connected);
    tick(&s,900000);assert(ap_owned&&ble_paused);
    kp_network_service_ap(false);tick(&s,900050);connected=true;tick(&s,900100);assert(!ap_owned&&!ble_paused&&!web_owned&&!leased);
    for(unsigned f=0;f<2;++f){
        ble_pause_fault=f==0?ESP_FAIL:ESP_OK;ap_start_fault=f==1?ESP_FAIL:ESP_OK;
        kp_network_service_ap(true);tick(&s,910000+f*10000);assert(!ap_owned&&!ble_paused);
        ble_pause_fault=ap_start_fault=0;kp_network_service_ap(false);tick(&s,910050+f*10000);connected=true;tick(&s,910100+f*10000);assert(!web_owned);
    }
    // A request on the boot deadline acquires ownership before network_tick.
    s=(service_t){0};kp_session_reset();kp_network_service_transfer(true);tick(&s,120000);assert(leased&&web_ready);
    kp_network_service_transfer(false);tick(&s,120050);assert(!leased&&!web_owned);
    s=(service_t){0};kp_session_reset();kp_network_service_config(true,true);tick(&s,120000);assert(leased&&ap_owned);
    kp_network_service_config(false,true);tick(&s,120050);assert(!leased&&!ap_owned);
    for(unsigned i=0;i<20;++i){
        connected=true;kp_network_service_transfer(true);tick(&s,1000000+i*100);assert(web_owned&&leased);
        kp_network_service_transfer(false);tick(&s,1000050+i*100);assert(!web_owned&&!leased);
    }
    puts("Manual LAN worker: no background HTTP, explicit mode/STA ownership, browser-independent lifetime, deadline admission, failed teardown, reconnect and SoftAP: PASS");
}
