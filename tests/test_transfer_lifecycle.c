#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fake_web.h"
static void quiet_log(const char *tag,const char *format,...){(void)tag;(void)format;}
#undef ESP_LOGI
#define ESP_LOGI(...) quiet_log(__VA_ARGS__)
unsigned esp_get_free_heap_size(void){return 0;}
unsigned esp_get_minimum_free_heap_size(void){return 0;}
size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 0;}
#include "../main/keyboard/keypad_network_service.c"
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task){(void)task;return 0;}
static bool has_wifi,connected,leased,suspended,web_owned,web_ready,ap_owned,ble_paused;
static unsigned web_starts,web_stops,ble_pauses;static esp_err_t web_start_fault,web_stop_fault,suspend_fault,ap_stop_fault;
static char current_ip[16]="192.168.1.50";
static kp_web_request_t request_callback;
int64_t esp_timer_get_time(void){return 0;}
void vTaskDelay(TickType_t ms){(void)ms;}
int xTaskCreate(void (*f)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *out){assert(f&&name&&stack==4096&&!arg&&priority==3);*out=(void *)1;return pdPASS;}
void kp_store_wifi_get(kp_wifi_config_t *out){kp_wifi_defaults(out);if(has_wifi)strcpy(out->ssid,"saved");}
void kp_network_keep_online(bool keep){leased=keep;}
esp_err_t kp_network_suspend(void){if(suspend_fault)return suspend_fault;assert(!web_owned);suspended=true;connected=false;return ESP_OK;}
void kp_network_resume(void){suspended=false;}
void kp_network_tick(uint32_t now){(void)now;assert(!ap_owned&&!suspended);}
bool kp_network_ip(char out[16]){if(connected)strcpy(out,current_ip);else out[0]=0;return connected;}
kp_network_status_t kp_network_status(void){return connected?KP_NET_SYNCED:KP_NET_CONNECTING;}
esp_err_t kp_transfer_web_start(const char *ip){assert(connected&&leased&&!ap_owned&&!ble_paused&&!web_owned&&!strcmp(ip,current_ip));++web_starts;if(web_start_fault)return web_start_fault;web_owned=web_ready=true;return ESP_OK;}
esp_err_t kp_transfer_web_stop(void){++web_stops;if(web_stop_fault)return web_stop_fault;web_owned=web_ready=false;return ESP_OK;}
bool kp_transfer_web_running(void){return web_owned;}
bool kp_transfer_web_ready(void){return web_ready;}
unsigned kp_transfer_web_peers(void){return 3;}
void kp_transfer_web_poll(uint32_t now){(void)now;assert(web_ready);}
esp_err_t kp_ble_pause(bool pause){++ble_pauses;ble_paused=pause;return ESP_OK;}
esp_err_t kp_web_start(kp_web_request_t cb){assert(suspended&&!web_owned&&ble_paused);ap_owned=true;request_callback=cb;return ESP_OK;}
esp_err_t kp_web_stop(void){if(ap_stop_fault)return ap_stop_fault;ap_owned=false;return ESP_OK;}
const char *kp_web_ssid(void){return "Passport-Keys-test";}
const char *kp_web_password(void){return "test-pass";}
void kp_wifi_setup_poll(uint32_t now){(void)now;assert(ap_owned);}
int main(void){
    service_t s={0};assert(kp_network_service_start()==ESP_OK);step(&s,0);assert(!s.view.transfer.enabled&&!leased&&!ble_pauses);
    kp_network_service_transfer(true);step(&s,1);assert(s.view.transfer.state==KP_TRANSFER_NO_WIFI&&!s.desired&&!leased);
    has_wifi=true;kp_network_service_transfer(true);step(&s,2);assert(leased&&s.desired&&s.view.transfer.enabled&&s.view.transfer.state==KP_TRANSFER_CONNECTING);
    connected=true;step(&s,1000);assert(web_ready&&s.view.transfer.state==KP_TRANSFER_READY&&s.view.transfer.peers==3&&!ble_pauses);
    step(&s,2000);assert(web_starts==1);connected=false;step(&s,3000);assert(!web_owned&&!s.view.transfer.ip[0]&&s.view.transfer.state==KP_TRANSFER_RECONNECTING&&leased);
    connected=true;strcpy(current_ip,"192.168.1.60");step(&s,4000);assert(web_ready&&web_starts==2&&!strcmp(s.view.transfer.ip,current_ip));
    web_stop_fault=ESP_FAIL;kp_network_service_transfer(false);step(&s,5000);assert(web_owned&&s.view.transfer.enabled&&s.view.transfer.state==KP_TRANSFER_STOPPING&&!s.desired);
    kp_network_service_transfer(true);step(&s,5100);assert(!s.desired&&web_starts==2);web_stop_fault=0;step(&s,6000);assert(!web_owned&&!leased&&!s.view.transfer.enabled&&!suspended);
    kp_network_service_transfer(true);step(&s,7000);connected=true;web_start_fault=ESP_FAIL;step(&s,8000);assert(s.view.transfer.state==KP_TRANSFER_FAILED&&!s.view.transfer.ip[0]);
    web_start_fault=0;step(&s,9000);assert(!web_owned);kp_network_service_transfer(true);step(&s,10000);assert(web_ready&&s.view.transfer.state==KP_TRANSFER_READY);
    // AP cannot start until both signaling and STA teardown succeed.
    suspend_fault=ESP_FAIL;kp_network_service_ap(true);step(&s,11000);assert(!web_owned&&s.view.transfer.enabled&&s.view.transfer.state==KP_TRANSFER_STOPPING&&!ap_owned&&!ble_paused);
    suspend_fault=0;step(&s,12000);assert(ap_owned&&ble_paused&&s.view.ap==KP_AP_ACTIVE&&!leased&&!s.view.transfer.enabled);
    ap_stop_fault=ESP_FAIL;request_callback(false);step(&s,13000);assert(ap_owned&&ble_paused&&s.view.ap==KP_AP_FAILED);
    ap_stop_fault=0;step(&s,14000);assert(!ap_owned&&!ble_paused&&!suspended&&s.view.ap==KP_AP_OFF);
    puts("Transfer lifecycle: no Wi-Fi, independent enable, reconnect/IP changes, retries, duplicate stop, BLE preservation and ordered AP handoff: PASS");
}
