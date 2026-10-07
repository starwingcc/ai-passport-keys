#include <assert.h>
#include <stdio.h>
#include <time.h>
static time_t test_time(time_t *out);
#define time test_time
#include "../main/keyboard/keypad_network.c"
#undef time

esp_event_base_t WIFI_EVENT = "wifi", IP_EVENT = "ip";
static kp_wifi_config_t settings;
static kp_clock_t clock_value;
static uint32_t test_now;
static time_t utc;
static unsigned fault, stop_fault, initialized, running, interfaces, handlers, sntp, connects;
static esp_netif_t netif;
static network_event_handler_t wifi_cb, ip_cb;
static void (*sync_cb)(struct timeval *);
static unsigned registration;

void kp_store_wifi_get(kp_wifi_config_t *out) { *out = settings; }
bool kp_store_time_sync(uint64_t epoch, uint64_t now) { return kp_clock_sync(&clock_value, epoch, now); }
static time_t test_time(time_t *out) { if (out) *out = utc; return utc; }
int64_t esp_timer_get_time(void) { return (int64_t)test_now * 1000; }
const char *esp_err_to_name(esp_err_t rc) { (void)rc; return "injected failure"; }
esp_netif_t *esp_netif_create_default_wifi_sta(void) {
    assert(!interfaces && !initialized && !running); registration = 0;
    if (fault == 1) return NULL;
    ++interfaces; return &netif;
}
void esp_netif_destroy_default_wifi(esp_netif_t *n) { assert(n == &netif && interfaces == 1 && !initialized && !running && !handlers && !sntp); --interfaces; }
esp_err_t esp_wifi_init(const wifi_init_config_t *c) { (void)c; assert(!initialized); if (fault == 2) return ESP_FAIL; ++initialized; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { assert(initialized && !running); if (stop_fault == 3) return ESP_FAIL; --initialized; return ESP_OK; }
esp_err_t esp_wifi_set_storage(int mode) { assert(mode == WIFI_STORAGE_RAM); return fault == 5 ? ESP_FAIL : ESP_OK; }
esp_err_t esp_wifi_set_mode(int mode) { assert(mode == WIFI_MODE_STA); return fault == 6 ? ESP_FAIL : ESP_OK; }
esp_err_t esp_wifi_set_config(int iface, const wifi_config_t *cfg) {
    assert(iface == WIFI_IF_STA && !memcmp(cfg->sta.ssid, settings.ssid, strlen(settings.ssid)));
    assert(!memcmp(cfg->sta.password, settings.password, strlen(settings.password)));
    assert(cfg->sta.threshold.authmode == (settings.password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN));
    return fault == 7 ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_wifi_start(void) { assert(initialized && !running); if (fault == 8) return ESP_FAIL; ++running; wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_STA_START, NULL); return ESP_OK; }
esp_err_t esp_wifi_stop(void) { assert(running && !sntp && !handlers); if (stop_fault == 2) return ESP_FAIL; --running; return ESP_OK; }
esp_err_t esp_wifi_connect(void) { assert(running); ++connects; return ESP_OK; }
esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id, network_event_handler_t cb, void *arg, esp_event_handler_instance_t *out) {
    (void)id; assert(!arg); ++registration;
    if (fault == registration + 2) return ESP_FAIL;
    if (base == WIFI_EVENT) { wifi_cb = cb; *out = &wifi_cb; }
    else { assert(base == IP_EVENT); ip_cb = cb; *out = &ip_cb; }
    ++handlers; return ESP_OK;
}
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id, esp_event_handler_instance_t h) {
    (void)id; assert(handlers && !sntp);
    if (stop_fault == 1) return ESP_FAIL;
    if (base == WIFI_EVENT) { assert(h == &wifi_cb); wifi_cb = NULL; }
    else { assert(base == IP_EVENT && h == &ip_cb); ip_cb = NULL; }
    --handlers; return ESP_OK;
}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *cfg) {
    assert(!sntp && running && cfg->start && !cfg->wait_for_sync && cfg->num_of_servers == 3);
    for (unsigned i = 0; i < 3; ++i) assert(cfg->servers[i] && cfg->servers[i][0]);
    if (fault == 9) return ESP_FAIL;
    ++sntp; sync_cb = cfg->sync_cb; return ESP_OK;
}
void esp_netif_sntp_deinit(void) { assert(sntp); --sntp; sync_cb = NULL; }
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *out){assert(n==&netif);out->ip.addr=0x3201a8c0U;return ESP_OK;}
static void tick(uint32_t now) { test_now = now; kp_network_tick(now); }
static void fresh(void) {
    fault = stop_fault = 0; kp_network_keep_online(false); assert(kp_network_suspend() == ESP_OK);
    kp_network_resume(); kp_wifi_defaults(&settings);
    strcpy(settings.ssid, "test-network"); strcpy(settings.password, "test-pass");
    connects = 0; clock_value = (kp_clock_t){0}; utc = 1791167040;
}
static void connected(uint32_t now) {
    tick(now); assert(kp_network_status() == KP_NET_CONNECTING);
    tick(now + 1); assert(connects == 1 && !sntp);
    ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    tick(now + 2); assert(kp_network_status() == KP_NET_SYNCING && sntp);
}
int main(void) {
    kp_wifi_defaults(&settings); tick(0); assert(kp_network_status() == KP_NET_DISABLED && !initialized);
    fresh(); connected(100);
    struct timeval tv = {.tv_sec = utc}; sync_cb(&tv); tick(110);
    assert(kp_network_status() == KP_NET_SYNCED && clock_value.epoch == (uint64_t)utc);
    assert(!interfaces && !initialized && !running && !handlers && !sntp);
    tick(110 + REFRESH_MS - 1); assert(!running);
    tick(110 + REFRESH_MS); assert(running); assert(kp_network_suspend() == ESP_OK);
    // A saved network is loaded anew after simulated power loss. Nothing
    // invents a timestamp before an actual valid SNTP notification.
    fresh(); connected(0); assert(!clock_value.epoch);
    tv.tv_sec = 1; sync_cb(&tv); tick(4); assert(!clock_value.epoch && sntp);
    tv.tv_sec = utc; sync_cb(&tv); tick(5); assert(clock_value.epoch == (uint64_t)utc);
    // Wrong password/unavailable router: bounded reconnects and timeout.
    fresh(); tick(0);
    for (unsigned i = 0; i < 10; ++i) {
        wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL); tick(1 + i * CONNECT_RETRY_MS);
    }
    assert(connects == 3); tick(ATTEMPT_MS); assert(kp_network_status() == KP_NET_FAILED && !running && !clock_value.epoch);
    tick(ATTEMPT_MS + RETRY_MS - 1); assert(!running);
    tick(ATTEMPT_MS + RETRY_MS); assert(running);
    // Loss of IP stops SNTP; DHCP recovery restarts it.
    fresh(); connected(0); wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
    tick(4000); assert(!sntp && kp_network_status() == KP_NET_CONNECTING);
    ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL); tick(4001); assert(sntp);
    assert(kp_network_suspend() == ESP_OK); tick(999999); assert(!running && !handlers && !sntp);
    kp_wifi_defaults(&settings); kp_network_resume(); tick(1000000); assert(kp_network_status() == KP_NET_DISABLED);
    // Every startup failure rolls back; a later attempt remains possible.
    for (unsigned f = 1; f <= 9; ++f) {
        fresh(); fault = f; tick(0);
        if (f == 9) { tick(1); ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL); tick(2); }
        assert(kp_network_status() == KP_NET_FAILED && !initialized && !running && !interfaces && !handlers && !sntp);
        fault = 0; tick(RETRY_MS + 2); assert(running);
    }
    // Cleanup failures retain ownership, prohibiting AP startup until retry.
    for (unsigned f = 1; f <= 3; ++f) {
        fresh(); connected(0); stop_fault = f;
        assert(kp_network_suspend() == ESP_FAIL && interfaces);
        tick(999999); assert(interfaces); // suspended means no restart
        stop_fault = 0; assert(kp_network_suspend() == ESP_OK && !interfaces && !initialized && !running && !handlers && !sntp);
    }
    fresh(); tick(UINT32_MAX - 100); tick(ATTEMPT_MS - 101); assert(kp_network_status() == KP_NET_FAILED);
    // Transfer leases keep STA alive after SNTP and without internet. IP loss
    // hides the address immediately, then DHCP recovery restores it.
    fresh();kp_network_keep_online(true);connected(0);char ip[16];assert(kp_network_ip(ip)&&!strcmp(ip,"192.168.1.50"));
    tv.tv_sec=utc;sync_cb(&tv);tick(5);assert(running&&interfaces&&!sntp&&kp_network_status()==KP_NET_SYNCED);
    tick(ATTEMPT_MS+100);assert(running&&kp_network_ip(ip));
    ip_cb(NULL,IP_EVENT,IP_EVENT_STA_LOST_IP,NULL);assert(!kp_network_ip(ip));
    ip_event_got_ip_t stale={.esp_netif=(void *)1};ip_cb(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,&stale);assert(!kp_network_ip(ip));
    ip_cb(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,NULL);assert(kp_network_ip(ip));
    wifi_cb(NULL,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,NULL);assert(!kp_network_ip(ip)&&!ip[0]);
    tick(ATTEMPT_MS+101);ip_cb(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,NULL);tick(ATTEMPT_MS+102);assert(running&&sntp&&kp_network_ip(ip));
    tick(2*ATTEMPT_MS+200);assert(running&&!sntp&&kp_network_ip(ip)); // SNTP timeout cannot stop transfer.
    kp_network_keep_online(false);assert(kp_network_suspend()==ESP_OK&&!running&&!interfaces);
    // A setup loan releases SNTP, keeps the netif and never races a candidate
    // test with automatic reconnect/timeout. Resume uses the latest IP event.
    fresh(); kp_network_keep_online(true); connected(0);
    unsigned before = connects; kp_network_setup_pause(true, 3);
    assert(!sntp && running && interfaces && handlers == 2);
    wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
    tick(3 * ATTEMPT_MS); assert(connects == before && running && !sntp);
    ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    kp_network_setup_pause(false, 3 * ATTEMPT_MS); tick(3 * ATTEMPT_MS + 1);
    assert(connects == before && running && sntp && kp_network_ip(ip));
    kp_network_keep_online(false); assert(kp_network_suspend() == ESP_OK);
    puts("Network time: cold boot, valid SNTP, retry/refresh, wrap, AP handoff and fault cleanup: PASS");
}
