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
static char current_ip[16]="192.168.1.11";
static kp_web_request_t request_callback;
static kp_transfer_wake_t wake_callback;
int64_t esp_timer_get_time(void){return (int64_t)test_now*1000;}
void vTaskDelay(TickType_t ms){(void)ms;}
int xTaskCreate(void (*f)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *out){assert(f&&name&&stack==4096&&!arg&&priority==3);*out=(void *)1;return pdPASS;}
void kp_store_wifi_get(kp_wifi_config_t *out){kp_wifi_defaults(out);if(has_wifi)strcpy(out->ssid,"saved");}
void kp_network_keep_online(bool keep){leased=keep;}
esp_err_t kp_network_suspend(void){if(suspend_fault)return suspend_fault;assert(!web_owned&&!ap_owned);suspended=true;connected=false;return ESP_OK;}
void kp_network_resume(void){suspended=false;}
void kp_network_tick(uint32_t now){(void)now;assert((!ap_owned||leased)&&!suspended);}
bool kp_network_ip(char out[16]){if(connected)strcpy(out,current_ip);else out[0]=0;return connected;}
kp_network_status_t kp_network_status(void){return connected?KP_NET_SYNCED:KP_NET_CONNECTING;}
esp_err_t kp_transfer_web_listen_start(const char *ip,kp_transfer_wake_t wake){assert(connected&&leased&&!ap_owned&&!ble_paused&&!strcmp(ip,current_ip));if(web_owned){if(partial_listener)return ESP_ERR_INVALID_STATE;web_up=true;return ESP_OK;}++web_starts;wake_callback=wake;if(listener_fault){if(partial_listener)web_owned=true;return listener_fault;}web_owned=web_up=true;return ESP_OK;}
esp_err_t kp_transfer_web_activate(uint64_t zero){assert(web_owned&&web_up&&!ap_owned&&!ble_paused);++activations;if(web_start_fault)return web_start_fault;if(!presence.allocated){presence=(kp_transfer_presence_t){.allocated=true,.generation=presence.generation+1,.zero_since_ms=zero};}web_ready=true;return ESP_OK;}
esp_err_t kp_transfer_web_deactivate(bool force){++deactivations;if(join_during_stop&&!force){join_during_stop=false;presence.online=1;return ESP_ERR_INVALID_STATE;}if(!force&&presence.allocated&&(presence.online||test_now-presence.zero_since_ms<KP_TRANSFER_QUIET_MS))return ESP_ERR_INVALID_STATE;if(release_fault){web_ready=false;return release_fault;}web_ready=false;presence.allocated=false;presence.online=0;return ESP_OK;}
esp_err_t kp_transfer_web_disconnect(void){web_ready=web_up=false;if(disconnect_fault)return disconnect_fault;if(presence.online)presence.zero_since_ms=test_now;presence.online=0;++presence.generation;return ESP_OK;}
esp_err_t kp_transfer_web_stop(void){++web_stops;if(web_stop_fault){partial_listener=true;return web_stop_fault;}web_owned=web_ready=web_up=partial_listener=false;presence.allocated=false;presence.online=0;return ESP_OK;}
bool kp_transfer_web_running(void){return web_owned;}
bool kp_transfer_web_ready(void){return web_ready&&web_up;}
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
esp_err_t kp_web_start_wifi(const char *ip,kp_web_request_t cb){assert(connected&&leased&&!suspended&&!ap_owned&&!web_owned&&!ble_paused&&!strcmp(ip,current_ip));if(config_start_fault)return config_start_fault;ap_owned=true;request_callback=cb;return ESP_OK;}
bool kp_wifi_setup_testing(void){return config_job.busy&&config_job.kind==KP_WIFI_TEST;}
void kp_web_update_ip(const char *ip){assert(ap_owned&&!strcmp(ip,current_ip));}
void kp_wifi_setup_poll(uint32_t now){(void)now;assert(ap_owned);}
static void tick(service_t *s,uint64_t now){test_now=now;step(s,now);}
static void offline(uint64_t now){assert(presence.allocated);presence.online=0;presence.zero_since_ms=now;}
int main(void){
    service_t s={0};assert(kp_network_service_start()==ESP_OK);tick(&s,0);assert(s.view.transfer.state==KP_TRANSFER_OFF&&!leased&&!ble_pauses);
    kp_network_service_transfer(true);tick(&s,1);assert(s.view.transfer.state==KP_TRANSFER_NO_WIFI&&!s.desired&&!leased);
    has_wifi=true;tick(&s,2);assert(leased&&!s.desired&&!web_owned);
    connected=true;tick(&s,50);assert(web_owned&&!presence.allocated&&s.view.transfer.state==KP_TRANSFER_LISTENING&&!ble_pauses);
    wake_callback(100);tick(&s,100);assert(web_ready&&s.view.transfer.enabled&&s.view.transfer.state==KP_TRANSFER_WAITING&&presence.zero_since_ms==100);
    wake_callback(1000);tick(&s,1000);assert(presence.zero_since_ms==100); // no join, repeated visit
    tick(&s,60099);assert(web_ready&&s.view.transfer.state==KP_TRANSFER_WAITING);
    tick(&s,60100);assert(!presence.allocated&&web_owned&&leased&&!suspended&&s.view.transfer.state==KP_TRANSFER_LISTENING);
    wake_callback(200000);tick(&s,200000);presence.online=2;tick(&s,200001);assert(s.view.transfer.state==KP_TRANSFER_READY);
    tick(&s,900000);assert(web_ready&&presence.allocated&&s.view.transfer.peers==2); // no file activity
    offline(900001);tick(&s,900002);assert(s.view.transfer.state==KP_TRANSFER_WAITING);
    presence.online=1;tick(&s,960000);assert(s.view.transfer.state==KP_TRANSFER_READY);
    offline(1100000);tick(&s,1159999);assert(web_ready);
    join_during_stop=true;tick(&s,1160000);assert(web_ready&&s.view.transfer.state==KP_TRANSFER_READY);
    offline(1300000);release_fault=ESP_FAIL;tick(&s,1360000);assert(presence.allocated&&web_owned&&leased&&s.view.transfer.state==KP_TRANSFER_STOPPING);
    unsigned starts=activations;kp_network_service_transfer(true);tick(&s,1360999);assert(activations==starts&&presence.allocated);
    release_fault=0;tick(&s,1361000);assert(!presence.allocated&&web_owned&&leased&&s.view.transfer.state==KP_TRANSFER_LISTENING&&web_starts==1&&!ble_pauses);
    // Activation errors retry in the original quiet window, keeping the entry.
    web_start_fault=ESP_ERR_NO_MEM;wake_callback(1500000);tick(&s,1500000);assert(web_owned&&!web_ready&&s.view.transfer.state==KP_TRANSFER_FAILED);
    starts=activations;tick(&s,1500999);assert(activations==starts);web_start_fault=0;tick(&s,1501000);assert(web_ready&&presence.zero_since_ms==1500000);
    // IP loss invalidates old sessions once, keeps catalogs and does not restart time.
    presence.online=1;tick(&s,1510000);connected=false;disconnect_fault=ESP_FAIL;tick(&s,1511000);assert(presence.allocated&&web_owned&&!s.view.transfer.ip[0]);
    disconnect_fault=0;tick(&s,1512000);assert(presence.zero_since_ms==1512000&&presence.allocated&&!presence.online);
    tick(&s,1542000);assert(presence.zero_since_ms==1512000);connected=true;tick(&s,1542001);assert(web_ready&&presence.zero_since_ms==1512000);
    tick(&s,1572000);assert(!presence.allocated&&s.view.transfer.state==KP_TRANSFER_LISTENING&&web_owned&&leased);
    // Manual enable/disable affects catalogs, never the standing radio lease.
    kp_network_service_transfer(true);tick(&s,1700000);presence.online=1;tick(&s,1700001);
    kp_network_service_transfer(false);tick(&s,1700002);assert(!presence.allocated&&web_owned&&leased&&!ble_pauses);
    // Configuration cannot take port 80/STA until failed stops recover.
    web_stop_fault=ESP_FAIL;kp_network_service_ap(true);tick(&s,1800000);assert(web_owned&&!ap_owned&&!ble_paused&&s.view.transfer.state==KP_TRANSFER_STOPPING);
    web_stop_fault=0;suspend_fault=ESP_FAIL;tick(&s,1801000);assert(!web_owned&&!ap_owned&&!ble_paused);
    suspend_fault=0;tick(&s,1802000);assert(ap_owned&&ble_paused&&s.config_ble_paused&&s.view.lan.state==KP_AP_ACTIVE);
    ap_stop_fault=ESP_FAIL;request_callback(false);tick(&s,1803000);assert(ap_owned&&ble_paused&&s.view.lan.state==KP_AP_FAILED);
    ap_stop_fault=0;tick(&s,1804000);assert(!ap_owned&&!ble_paused&&!suspended&&s.view.lan.state==KP_AP_OFF&&leased);
    connected=true;tick(&s,1804001);assert(web_owned&&s.view.transfer.state==KP_TRANSFER_LISTENING);
    // LAN configuration retains Bluetooth; tests temporarily own the station.
    unsigned pauses=ble_pauses;kp_network_service_config(true,true);tick(&s,1900000);assert(s.view.lan.state==KP_AP_STARTING&&leased&&!web_owned&&!ap_owned);
    connected=true;tick(&s,1901000);assert(ap_owned&&s.view.lan.state==KP_AP_ACTIVE&&ble_pauses==pauses);
    config_job=(kp_wifi_job_t){.kind=KP_WIFI_TEST,.busy=true};connected=false;tick(&s,1902000);assert(ap_owned&&s.config_testing&&s.view.lan.state==KP_AP_ACTIVE);
    connected=true;config_job.busy=false;tick(&s,1903000);assert(ap_owned&&!s.config_testing&&!strcmp(s.view.lan.ip,current_ip));
    kp_network_service_config(false,true);tick(&s,1904000);connected=true;tick(&s,1904001);assert(!ap_owned&&web_owned&&!presence.allocated&&s.view.transfer.state==KP_TRANSFER_LISTENING&&ble_pauses==pauses);
    // Partial HTTP starts and failed httpd_stop retain ownership until retry.
    assert(kp_transfer_web_stop()==ESP_OK);listener_fault=ESP_FAIL;partial_listener=true;tick(&s,2000000);assert(web_owned&&s.view.transfer.state==KP_TRANSFER_FAILED);
    web_stop_fault=ESP_FAIL;tick(&s,2001000);assert(web_owned&&!ap_owned);web_stop_fault=listener_fault=0;tick(&s,2002000);assert(kp_transfer_web_listening());
    // Config failures roll back BLE pauses, including a pause reporting failure.
    for(unsigned f=0;f<2;++f){
        ble_pause_fault=f==0?ESP_FAIL:ESP_OK;ap_start_fault=f==1?ESP_FAIL:ESP_OK;
        kp_network_service_ap(true);tick(&s,2100000+f*10000);assert(!ap_owned&&!ble_paused&&!s.config_ble_paused&&s.view.lan.state==KP_AP_FAILED);
        ble_pause_fault=ap_start_fault=0;kp_network_service_ap(false);tick(&s,2101000+f*10000);connected=true;tick(&s,2101001+f*10000);assert(web_owned&&!suspended);
    }
    kp_network_service_ap(true);tick(&s,2200000);assert(ap_owned&&ble_paused);
    ble_resume_fault=ESP_FAIL;kp_network_service_ap(false);tick(&s,2201000);assert(!ap_owned&&ble_paused&&s.config_ble_paused);
    ble_resume_fault=0;tick(&s,2202000);assert(!ble_paused&&!suspended&&s.view.lan.state==KP_AP_OFF);
    // Worker time remains 64-bit across the old 49-day counter boundary.
    connected=true;tick(&s,(uint64_t)UINT32_MAX-50);wake_callback((uint64_t)UINT32_MAX-10);tick(&s,(uint64_t)UINT32_MAX-10);
    tick(&s,(uint64_t)UINT32_MAX+59989);assert(presence.allocated);tick(&s,(uint64_t)UINT32_MAX+59990);assert(!presence.allocated&&web_owned);
    puts("Transfer worker: standing STA/listener, no-join timeout, online-only lifetime, exact 60s, reconnect, retry ownership, ordered configuration handoff, BLE preservation and 64-bit time: PASS");
}
