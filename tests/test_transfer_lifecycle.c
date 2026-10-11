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
static unsigned web_starts,activations,deactivations,web_stops,ble_pauses;
static esp_err_t web_start_fault,listener_fault,web_stop_fault,release_fault,disconnect_fault,suspend_fault,ap_stop_fault,config_start_fault,ble_pause_fault,ble_resume_fault,ap_start_fault;
static unsigned config_polls;
static kp_wifi_job_t config_job;
static kp_transfer_presence_t presence;
static bool join_during_stop,partial_listener;
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
void kp_network_tick(uint32_t now){(void)now;assert((!ap_owned||leased)&&!suspended);}
bool kp_network_ip(char out[16]){if(connected)strcpy(out,current_ip);else out[0]=0;return connected;}
kp_network_status_t kp_network_status(void){return connected?KP_NET_SYNCED:KP_NET_CONNECTING;}
esp_err_t kp_transfer_web_listen_start(const char *ip){assert(connected&&leased&&!ap_owned&&!ble_paused&&!strcmp(ip,current_ip));if(web_owned){if(partial_listener)return ESP_ERR_INVALID_STATE;web_up=true;return ESP_OK;}++web_starts;if(listener_fault){if(partial_listener)web_owned=true;return listener_fault;}web_owned=web_up=true;return ESP_OK;}
esp_err_t kp_transfer_web_activate(uint64_t zero){kp_session_view_t current;kp_session_get(&current);server_generation=current.generation;assert(web_owned&&web_up&&!ap_owned&&!ble_paused);++activations;if(web_start_fault)return web_start_fault;if(!presence.allocated){presence=(kp_transfer_presence_t){.allocated=true,.generation=presence.generation+1,.zero_since_ms=zero};}web_ready=true;return ESP_OK;}
esp_err_t kp_transfer_web_deactivate(bool force){++deactivations;if(join_during_stop&&!force){join_during_stop=false;presence.online=1;return ESP_ERR_INVALID_STATE;}if(!force&&presence.allocated&&(presence.online||presence.joining))return ESP_ERR_INVALID_STATE;if(release_fault){web_ready=false;return release_fault;}web_ready=false;presence.allocated=false;presence.online=0;return ESP_OK;}
esp_err_t kp_transfer_web_disconnect(void){web_ready=web_up=false;if(disconnect_fault)return disconnect_fault;if(presence.online)presence.zero_since_ms=test_now;presence.online=0;++presence.generation;return ESP_OK;}
esp_err_t kp_transfer_web_stop(void){++web_stops;if(web_stop_fault){partial_listener=true;return web_stop_fault;}web_owned=web_ready=web_up=partial_listener=false;presence.allocated=false;presence.online=0;return ESP_OK;}
bool kp_transfer_web_running(void){return web_owned;}
bool kp_transfer_web_ready(void){return web_ready&&web_up&&kp_session_current(KP_SESSION_TRANSFER,server_generation);}
bool kp_transfer_web_listening(void){return web_owned&&web_up&&!partial_listener;}
void kp_transfer_web_presence(kp_transfer_presence_t *out){*out=presence;}
unsigned kp_transfer_web_peers(void){return presence.online;}
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
static void tick(service_t *s,uint64_t now){test_now=now;kp_session_poll(now);kp_session_view_t current;kp_session_get(&current);presence.online=current.mode==KP_SESSION_TRANSFER?current.online:0;presence.joining=current.mode==KP_SESSION_TRANSFER?current.joining:0;step(s,now);}
static uint32_t open_mode(kp_session_mode_t mode,uint64_t now,uint32_t *generation){test_now=now;uint32_t lease=kp_session_open(mode,now,generation);assert(lease);return lease;}
int main(void){
    service_t s={0};assert(kp_network_service_start()==ESP_OK);tick(&s,0);assert(!leased&&!web_owned);
    has_wifi=true;connected=true;tick(&s,10);assert(web_owned&&leased&&!presence.allocated);
    unsigned idle_releases=deactivations;tick(&s,11);tick(&s,12);assert(deactivations==idle_releases);
    uint32_t g;uint32_t a=open_mode(KP_SESSION_TRANSFER,20,&g);tick(&s,20);assert(web_ready&&s.view.transfer.state==KP_TRANSFER_WAITING);
    tick(&s,30019);assert(presence.allocated);tick(&s,30020);assert(!presence.allocated&&web_owned&&leased);
    a=open_mode(KP_SESSION_TRANSFER,40000,&g);tick(&s,40000);assert(kp_session_join(KP_SESSION_TRANSFER,g,a,40001));tick(&s,40001);
    uint32_t b=open_mode(KP_SESSION_TRANSFER,40002,&g);assert(kp_session_join(KP_SESSION_TRANSFER,g,b,40003));tick(&s,40003);assert(s.view.transfer.peers==2);
    kp_session_leave(KP_SESSION_TRANSFER,g,a,40004);tick(&s,40004);assert(presence.allocated&&s.view.transfer.peers==1);
    kp_session_leave(KP_SESSION_TRANSFER,g,b,40005);tick(&s,40005);assert(!presence.allocated&&web_owned&&leased);
    a=open_mode(KP_SESSION_TRANSFER,50000,&g);tick(&s,50000);assert(kp_session_join(KP_SESSION_TRANSFER,g,a,50001));tick(&s,50001);
    unsigned radio_stops=suspends,ble_calls=ble_pauses;
    uint32_t config_g,c=open_mode(KP_SESSION_CONFIG,50002,&config_g);tick(&s,50002);assert(web_owned&&!ap_owned);
    tick(&s,50302);assert(ap_owned&&!web_owned&&connected&&suspends==radio_stops&&ble_pauses==ble_calls);
    assert(kp_session_join(KP_SESSION_CONFIG,config_g,c,50303));tick(&s,50303);
    kp_network_view_t view;kp_network_service_get(&view);assert(view.lan.online==1&&view.lan.listening&&!view.transfer.peers);
    a=open_mode(KP_SESSION_TRANSFER,50304,&g);tick(&s,50304);tick(&s,50354);
    assert(web_owned&&!ap_owned&&web_ready&&connected&&suspends==radio_stops&&ble_pauses==ble_calls);
    assert(!kp_session_ping(KP_SESSION_CONFIG,config_g,c,50355));assert(kp_session_join(KP_SESSION_TRANSFER,g,a,50355));tick(&s,50355);
    kp_network_service_get(&view);assert(view.transfer.peers==1&&view.lan.listening&&!view.lan.online);
    c=open_mode(KP_SESSION_CONFIG,50356,&config_g);web_stop_fault=ESP_FAIL;tick(&s,50356);tick(&s,50656);assert(web_owned&&!ap_owned);
    web_stop_fault=0;tick(&s,51656);assert(ap_owned&&!web_owned);assert(kp_session_join(KP_SESSION_CONFIG,config_g,c,51657));tick(&s,51657);
    kp_session_leave(KP_SESSION_CONFIG,config_g,c,51658);ap_stop_fault=ESP_FAIL;tick(&s,51658);assert(ap_owned&&!web_owned);
    ap_stop_fault=0;tick(&s,51708);assert(!ap_owned&&web_owned&&connected&&suspends==radio_stops);
    // A heartbeat expiration has no second timer.
    a=open_mode(KP_SESSION_TRANSFER,60000,&g);tick(&s,60000);assert(kp_session_join(KP_SESSION_TRANSFER,g,a,60001));tick(&s,60001);
    tick(&s,180000);assert(presence.allocated);tick(&s,180001);assert(!presence.allocated&&web_owned);
    // Activation/release failures preserve ownership and retry without radio churn.
    a=open_mode(KP_SESSION_TRANSFER,200000,&g);web_start_fault=ESP_FAIL;tick(&s,200000);assert(web_owned&&!web_ready);
    web_start_fault=0;tick(&s,201000);assert(web_ready);assert(kp_session_join(KP_SESSION_TRANSFER,g,a,201001));tick(&s,201001);
    kp_session_leave(KP_SESSION_TRANSFER,g,a,201002);release_fault=ESP_FAIL;tick(&s,201002);assert(presence.allocated&&web_owned);
    kp_network_service_get(&view);assert(!view.lan.listening&&view.transfer.state==KP_TRANSFER_STOPPING);
    release_fault=0;tick(&s,202002);assert(!presence.allocated&&web_owned);
    // SoftAP keeps its existing explicit lifecycle and BLE ownership.
    kp_network_service_ap(true);tick(&s,210000);assert(ap_owned&&ble_paused&&!connected);
    kp_network_service_ap(false);tick(&s,210050);connected=true;tick(&s,210100);assert(!ap_owned&&!ble_paused&&web_owned);
    for(unsigned f=0;f<2;++f){
        ble_pause_fault=f==0?ESP_FAIL:ESP_OK;ap_start_fault=f==1?ESP_FAIL:ESP_OK;
        kp_network_service_ap(true);tick(&s,220000+f*10000);assert(!ap_owned&&!ble_paused);
        ble_pause_fault=ap_start_fault=0;kp_network_service_ap(false);tick(&s,220050+f*10000);connected=true;tick(&s,220100+f*10000);assert(web_owned);
    }
    a=open_mode(KP_SESSION_TRANSFER,(uint64_t)UINT32_MAX-10,&g);tick(&s,(uint64_t)UINT32_MAX-10);
    assert(kp_session_join(KP_SESSION_TRANSFER,g,a,(uint64_t)UINT32_MAX));tick(&s,(uint64_t)UINT32_MAX);
    kp_session_leave(KP_SESSION_TRANSFER,g,a,(uint64_t)UINT32_MAX+1);tick(&s,(uint64_t)UINT32_MAX+1);assert(!presence.allocated&&web_owned);
    puts("LAN worker: immediate last leave/heartbeat, 30s admission, bidirectional HTTP ownership, standing STA/BLE, stale generations, failed cleanup retry, SoftAP and 64-bit time: PASS");
}
