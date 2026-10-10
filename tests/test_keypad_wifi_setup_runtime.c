#include <assert.h>
#include <stdio.h>
#include "esp_netif_sntp.h"
#include "../main/keyboard/keypad_wifi_setup_runtime.c"

esp_event_base_t WIFI_EVENT = "wifi", IP_EVENT = "ip";
static int locked;
static unsigned handlers, fault, connects, config_writes, sntp, clears, disconnects;
static bool scan_running, associated, setup_paused;
static bool candidate_dhcp;
static unsigned address_fault;
static network_event_handler_t wifi_cb, ip_cb;
static wifi_config_t station_config;
static wifi_ap_record_t records[96], associated_ap;
static uint16_t record_count, record_at;
static uint32_t test_now;

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &locked; }
int xSemaphoreTake(SemaphoreHandle_t h, int timeout) { (void)timeout; assert(h == &locked && !locked); locked = 1; return 1; }
void xSemaphoreGive(SemaphoreHandle_t h) { assert(h == &locked && locked); locked = 0; }
int64_t esp_timer_get_time(void) { return (int64_t)test_now * 1000; }
bool kp_store_time_sync(uint64_t epoch, uint64_t now) { (void)epoch; (void)now; assert(!"Connection tests must not change the clock"); return false; }
esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id, network_event_handler_t cb, void *arg, esp_event_handler_instance_t *h) {
    (void)id; assert(!arg && !locked);
    if (base == WIFI_EVENT) { if (fault == 1) return ESP_FAIL; wifi_cb = cb; *h = &wifi_cb; }
    else { assert(base == IP_EVENT); if (fault == 2) return ESP_FAIL; ip_cb = cb; *h = &ip_cb; }
    ++handlers; return ESP_OK;
}
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id, esp_event_handler_instance_t h) {
    (void)id; assert(!locked && handlers);
    if (fault == 12) return ESP_FAIL;
    if (base == WIFI_EVENT) { assert(h == &wifi_cb); wifi_cb = NULL; }
    else { assert(base == IP_EVENT && h == &ip_cb); ip_cb = NULL; }
    --handlers; return ESP_OK;
}
esp_err_t esp_wifi_scan_start(const void *cfg, bool block) {
    assert(!locked && !cfg && !block && !scan_running);
    if (fault == 3) return ESP_FAIL;
    scan_running = true; record_at = 0; return ESP_OK;
}
esp_err_t esp_wifi_scan_stop(void) { assert(!locked); if (fault == 11) return ESP_FAIL; scan_running = false; return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *n) { assert(!locked); if (fault == 4) return ESP_FAIL; *n = record_count; return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *ap) {
    assert(!locked && record_at < record_count);
    if (fault == 5) return ESP_FAIL;
    *ap = records[record_at++]; return ESP_OK;
}
esp_err_t esp_wifi_clear_ap_list(void) { assert(!locked); if (fault == 6) return ESP_FAIL; record_count = record_at = 0; ++clears; return ESP_OK; }
esp_err_t esp_wifi_set_config(int iface, const wifi_config_t *cfg) {
    assert(!locked && iface == WIFI_IF_STA);
    if (fault == 7) return ESP_FAIL;
    station_config = *cfg; ++config_writes; return ESP_OK;
}
void kp_network_setup_pause(bool pause, uint32_t now) { (void)now; setup_paused = pause; }
esp_err_t kp_network_setup_test_address(bool candidate) { assert(setup_paused&&!associated);if(address_fault==(candidate?1U:2U))return ESP_FAIL;candidate_dhcp=candidate;return ESP_OK; }
esp_err_t esp_wifi_get_config(int iface, wifi_config_t *cfg) { assert(iface == WIFI_IF_STA); if (fault == 13) return ESP_FAIL; *cfg = station_config; return ESP_OK; }
esp_err_t esp_wifi_connect(void) { assert(!locked && !scan_running); if (fault == 8) return ESP_FAIL; ++connects; return ESP_OK; }
esp_err_t esp_wifi_disconnect(void) {
    assert(!locked);
    if (fault == 10) return ESP_FAIL;
    ++disconnects;
    associated = false;
    wifi_event_sta_disconnected_t data = {.reason = 8};
    wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &data); return ESP_OK;
}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap) {
    assert(!locked); if (!associated) return ESP_ERR_WIFI_NOT_CONNECT;
    *ap = associated_ap; return ESP_OK;
}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *cfg) { (void)cfg; assert(!"Connection tests must not start SNTP"); return ESP_FAIL; }
void esp_netif_sntp_deinit(void) { assert(!"Connection tests do not own SNTP"); }
static void tick(uint32_t now) { test_now = now; kp_wifi_setup_poll(now); }
static kp_wifi_job_t get(void) { kp_wifi_job_t view; kp_wifi_setup_get(&view); return view; }
static void fresh(void) {
    fault = 0; assert(kp_wifi_setup_stop() == ESP_OK && !handlers && !sntp && !scan_running);
    assert(kp_wifi_setup_start(false) == ESP_OK && handlers == 2);
    assert(kp_wifi_setup_start(false) == ESP_ERR_INVALID_STATE && handlers == 2);
    assert(!get().busy && !setup_paused); associated = false;
    connects = config_writes = clears = disconnects = 0; record_count = 0;
}
static uint32_t test_begin(uint32_t now) {
    kp_wifi_config_t wifi; kp_wifi_defaults(&wifi); strcpy(wifi.ssid, "candidate-network"); strcpy(wifi.password, "test-pass");
    uint32_t id; assert(kp_wifi_setup_test(&wifi, &id) == ESP_OK && get().busy && get().state == KP_WIFI_QUEUED);
    uint32_t other; assert(kp_wifi_setup_scan(&other) == ESP_ERR_INVALID_STATE);
    tick(now); assert(get().state == KP_WIFI_CONNECTING);
    return id;
}
static void got_ip(uint32_t now) {
    associated = true; memset(&associated_ap, 0, sizeof(associated_ap));
    memcpy(associated_ap.ssid, station_config.sta.ssid, 32);
    ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL); tick(now);
    assert(get().connected && get().state == KP_WIFI_CONNECTING && s_finishing && !sntp);
}
static void settle(uint32_t now) { tick(now); tick(now + 100); }
static void scanned(uint32_t now, unsigned status) {
    scan_running = false; wifi_event_sta_scan_done_t event = {.status = status};
    wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &event); tick(now); tick(now + 1);
}
static void lan_fresh(void) {
    fresh(); assert(kp_wifi_setup_stop() == ESP_OK);
    assert(kp_wifi_setup_start(true) == ESP_OK);
    station_config = (wifi_config_t){0};
    strcpy((char *)station_config.sta.ssid, "original-network");
    strcpy((char *)station_config.sta.password, "original-pass");
    associated = true; associated_ap = (wifi_ap_record_t){0};
    memcpy(associated_ap.ssid, station_config.sta.ssid, 32);
}
static void restored(uint32_t now, kp_wifi_job_state_t result) {
    assert(get().busy && get().state == KP_WIFI_RESTORING && setup_paused);
    assert(!strcmp((char *)station_config.sta.ssid, "original-network"));
    assert(!strcmp((char *)station_config.sta.password, "original-pass"));
    associated = true; memcpy(associated_ap.ssid, station_config.sta.ssid, 32);
    memset(associated_ap.ssid + 17, 0x7f, 15); // Bytes after the SSID terminator are unspecified.
    ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL); tick(now);
    assert(!get().busy && get().state == result && !get().restore_failed && !setup_paused && !candidate_dhcp);
    assert(!s_original.sta.password[0] && !s_candidate.password[0]);
}
int main(void) {
    uint32_t id; assert(kp_wifi_setup_scan(&id) == ESP_ERR_INVALID_STATE);
    for (unsigned f = 1; f <= 2; ++f) { fault = f; assert(kp_wifi_setup_start(false) == ESP_FAIL); fault = 0; assert(kp_wifi_setup_stop() == ESP_OK && !handlers); }
    fresh(); assert(kp_wifi_setup_scan(&id) == ESP_OK); tick(0); assert(scan_running && get().state == KP_WIFI_SCANNING);
    record_count = 80;
    for (unsigned i = 0; i < record_count; ++i) {
        snprintf((char *)records[i].ssid, 33, "network-%u", i/2); records[i].rssi = -20-(int)i; records[i].authmode = WIFI_AUTH_WPA2_PSK;
    }
    scanned(1, 0); assert(!get().busy && get().state == KP_WIFI_SCAN_DONE && get().count == 16 && clears == 1 && !record_count);
    assert(!strcmp(get().networks[0].ssid, "network-0"));
    assert(kp_wifi_setup_scan(&id) == ESP_OK); tick(10); scanned(11, 0); assert(get().count == 0 && !get().busy);
    fresh(); test_begin(100); assert(connects == 1 && !strcmp((char *)station_config.sta.password, "test-pass"));
    got_ip(101); settle(102);
    assert(!get().busy && get().state == KP_WIFI_VERIFIED && get().connected);
    assert(!station_config.sta.password[0] && !s_candidate.password[0]);
    // A delayed IP notification for a different association cannot validate a password.
    fresh(); test_begin(0); associated = true; strcpy((char *)associated_ap.ssid, "previous-network");
    ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL); tick(1); assert(!get().connected && !sntp);
    tick(30000); settle(30001); assert(get().state == KP_WIFI_TIMEOUT && !get().connected);
    const unsigned reasons[] = {202,15,204,201,212,210,211,2,4,203};
    for (unsigned i = 0; i < sizeof(reasons)/sizeof(reasons[0]); ++i) {
        fresh(); test_begin(0); wifi_event_sta_disconnected_t event = {.reason = reasons[i]};
        associated = false; wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event); tick(1); settle(2);
        assert(!get().busy && get().state == kp_wifi_failure(reasons[i]) && !get().connected && get().reason == reasons[i]);
    }
    // A working LAN without Internet/NTP still passes immediately at DHCP.
    fresh(); test_begin(0); got_ip(1); settle(2);
    assert(get().state == KP_WIFI_VERIFIED && get().connected && !sntp && !get().busy);
    // Scan start/result/driver errors release every record and allow retry.
    for (unsigned f = 3; f <= 6; ++f) {
        fresh(); record_count = f == 3 ? 0 : 1; records[0].ssid[0] = 0; assert(kp_wifi_setup_scan(&id) == ESP_OK);
        fault = f; tick(0); if (f != 3) scanned(1, 0);
        fault = 0; settle(2); assert(!get().busy && get().state == KP_WIFI_ERROR && !scan_running && !record_count);
        assert(kp_wifi_setup_scan(&id) == ESP_OK); tick(3); scanned(4, 0); assert(get().state == KP_WIFI_SCAN_DONE);
    }
    for (unsigned f = 7; f <= 8; ++f) { fresh(); fault = f; test_begin(0); fault = 0; settle(1); assert(get().state == KP_WIFI_ERROR && !get().busy); }
    fresh(); assert(kp_wifi_setup_scan(&id) == ESP_OK); tick(UINT32_MAX-10); tick(14989); settle(14990); assert(get().state == KP_WIFI_TIMEOUT && !scan_running);
    // Queued secrets are cleared on exit, and failed teardown keeps ownership.
    fresh(); kp_wifi_config_t wifi; kp_wifi_defaults(&wifi); strcpy(wifi.ssid, "test-network"); strcpy(wifi.password, "test-pass");
    assert(kp_wifi_setup_test(&wifi, &id) == ESP_OK); assert(kp_wifi_setup_stop() == ESP_OK && !s_candidate.password[0]);
    for (unsigned f = 10; f <= 12; ++f) {
        fresh(); if (f == 11) { assert(kp_wifi_setup_scan(&id) == ESP_OK); tick(0); } else test_begin(0);
        fault = f; assert(kp_wifi_setup_stop() == ESP_FAIL);
        fault = 0; assert(kp_wifi_setup_stop() == ESP_OK && !handlers && !sntp && !scan_running && !get().busy);
    }
    // Connected LAN scanning preserves the original station and its credentials.
    lan_fresh(); assert(kp_wifi_setup_scan(&id) == ESP_OK); tick(0);
    assert(associated && setup_paused && !disconnects && !config_writes);
    scanned(1, 0); assert(get().state == KP_WIFI_SCAN_DONE && !get().busy && associated && !setup_paused);
    assert(!disconnects && !config_writes);
    // Successful, failed, timed-out and same-SSID tests all restore the session
    // network before releasing ownership; none save the candidate credentials.
    lan_fresh(); test_begin(0); tick(99); assert(!connects && setup_paused);
    tick(100); got_ip(101); settle(102);
    restored(203, KP_WIFI_VERIFIED); assert(get().connected && connects == 2);
    lan_fresh(); test_begin(0); tick(100);
    wifi_event_sta_disconnected_t wrong = {.reason = 202};
    wifi_cb(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wrong); tick(101); settle(102);
    restored(203, KP_WIFI_AUTH_FAILED); assert(!get().connected && get().reason == 202);
    lan_fresh(); test_begin(0); tick(100); tick(30100); settle(30101);
    restored(30202, KP_WIFI_TIMEOUT);
    lan_fresh(); kp_wifi_config_t same; kp_wifi_defaults(&same);
    strcpy(same.ssid, "original-network"); strcpy(same.password, "other-pass");
    assert(kp_wifi_setup_test(&same, &id) == ESP_OK); tick(0); tick(100); got_ip(101);
    settle(102); restored(203, KP_WIFI_VERIFIED); assert(get().connected);
    // A lost DHCP event times out restoration without mistaking the candidate
    // association for the original one or losing the candidate test result.
    lan_fresh(); test_begin(0); tick(100); got_ip(101); settle(102);
    associated = true; strcpy((char *)associated_ap.ssid, "candidate-network");
    ip_cb(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL); tick(203); assert(get().busy);
    tick(30202); assert(!get().busy && get().restore_failed && get().state == KP_WIFI_VERIFIED && !setup_paused);
    // Driver faults before borrowing credentials and during restore release
    // the lease, preserve the old configuration and erase temporary secrets.
    lan_fresh(); fault = 13; test_begin(0); fault = 0; tick(1);
    assert(!get().busy && get().state == KP_WIFI_ERROR && associated && !setup_paused && !s_candidate.password[0]);
    lan_fresh(); test_begin(0); tick(100); got_ip(101); tick(102);
    fault = 8; tick(202); fault = 0;
    assert(!get().busy && get().restore_failed && !setup_paused && !strcmp((char *)station_config.sta.password, "original-pass"));
    lan_fresh(); test_begin(0); tick(100); fault = 10; assert(kp_wifi_setup_stop() == ESP_FAIL);
    fault = 0; assert(kp_wifi_setup_stop() == ESP_OK && !handlers && !setup_paused && !s_original.sta.password[0]);
    // A failed DHCP loan or static restoration keeps setup ownership until
    // the original address can be restored, before reconnecting the station.
    lan_fresh();test_begin(0);address_fault=1;tick(100);address_fault=0;settle(101);
    restored(202,KP_WIFI_ERROR);
    lan_fresh();test_begin(0);tick(100);assert(candidate_dhcp);got_ip(101);tick(102);
    address_fault=2;tick(202);assert(get().busy&&setup_paused&&candidate_dhcp&&!associated);
    address_fault=0;tick(203);restored(204,KP_WIFI_VERIFIED);
    // Full-length SSIDs are valid and never indexed beyond the driver field.
    fresh(); kp_wifi_defaults(&same); memset(same.ssid, 'x', 32); strcpy(same.password, "test-pass");
    assert(kp_wifi_setup_test(&same, &id) == ESP_OK); tick(0); got_ip(1); settle(2);
    assert(get().connected && get().state == KP_WIFI_VERIFIED);
    puts("Wi-Fi setup runtime: LAN/background scans, station restoration, failures/timeouts, secret cleanup and teardown: PASS");
}
