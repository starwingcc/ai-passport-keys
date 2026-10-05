#include "keypad_network.h"
#include "keypad_store.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <string.h>
#include <time.h>

#define ATTEMPT_MS 60000U
#define RETRY_MS 300000U
#define REFRESH_MS 21600000U
#define CONNECT_RETRY_MS 3000U

static esp_netif_t *s_netif;
static esp_event_handler_instance_t s_wifi_handler, s_ip_handler;
static bool s_initialized, s_started, s_sntp, s_suspended, s_scheduled;
static uint32_t s_attempt_at, s_next_at, s_connect_at;
static unsigned s_connects;
static atomic_bool s_got_ip, s_reconnect, s_synced;
static atomic_int s_status;

kp_network_status_t kp_network_status(void) { return atomic_load(&s_status); }
static void event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)data;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) atomic_store(&s_got_ip, true);
    else if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        atomic_store(&s_got_ip, false); atomic_store(&s_reconnect, true);
    }
}
static void time_synced(struct timeval *tv) {
    if (tv->tv_sec >= (time_t)KP_TIME_MIN && tv->tv_sec <= (time_t)KP_TIME_MAX)
        atomic_store(&s_synced, true);
}
static esp_err_t stop(void) {
    // Stop the TCP/IP callback before destroying its interface. A failed Wi-Fi
    // teardown retains ownership and is retried, never handing it to the AP.
    if (s_sntp) { esp_netif_sntp_deinit(); s_sntp = false; }
    if (s_wifi_handler) {
        esp_err_t rc = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_handler);
        if (rc != ESP_OK) return rc;
        s_wifi_handler = NULL;
    }
    if (s_ip_handler) {
        esp_err_t rc = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_handler);
        if (rc != ESP_OK) return rc;
        s_ip_handler = NULL;
    }
    if (s_started) {
        esp_err_t rc = esp_wifi_stop();
        if (rc != ESP_OK) return rc;
        s_started = false;
    }
    if (s_initialized) {
        esp_err_t rc = esp_wifi_deinit();
        if (rc != ESP_OK) return rc;
        s_initialized = false;
    }
    if (s_netif) { esp_netif_destroy_default_wifi(s_netif); s_netif = NULL; }
    atomic_store(&s_got_ip, false); atomic_store(&s_reconnect, false); atomic_store(&s_synced, false);
    return ESP_OK;
}
esp_err_t kp_network_suspend(void) {
    s_suspended = true;
    esp_err_t rc = stop();
    if (rc == ESP_OK) atomic_store(&s_status, KP_NET_PAUSED);
    return rc;
}
void kp_network_resume(void) { s_suspended = false; s_scheduled = false; }
static esp_err_t start(const kp_wifi_config_t *wifi, uint32_t now) {
    s_netif = esp_netif_create_default_wifi_sta();
    if (!s_netif) return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t rc = esp_wifi_init(&init);
    if (rc != ESP_OK) return rc;
    s_initialized = true;
    rc = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL, &s_wifi_handler);
    if (rc == ESP_OK) rc = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, NULL, &s_ip_handler);
    wifi_config_t cfg = {0};
    // A valid SSID can occupy all 32 bytes; do not truncate with snprintf.
    memcpy(cfg.sta.ssid, wifi->ssid, strlen(wifi->ssid));
    memcpy(cfg.sta.password, wifi->password, strlen(wifi->password));
    cfg.sta.threshold.authmode = wifi->password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    if (rc == ESP_OK) rc = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (rc == ESP_OK) rc = esp_wifi_set_mode(WIFI_MODE_STA);
    if (rc == ESP_OK) rc = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (rc == ESP_OK) rc = esp_wifi_start();
    if (rc != ESP_OK) return rc;
    s_started = true; s_attempt_at = now; s_connect_at = now - CONNECT_RETRY_MS; s_connects = 0;
    atomic_store(&s_status, KP_NET_CONNECTING);
    ESP_LOGI("keypad_time", "Network time attempt started");
    return ESP_OK;
}
static void finish(uint32_t now, bool success) {
    atomic_store(&s_status, success ? KP_NET_SYNCED : KP_NET_FAILED);
    s_next_at = now + (success ? REFRESH_MS : RETRY_MS); s_scheduled = true;
    esp_err_t rc = stop();
    if (rc != ESP_OK) ESP_LOGW("keypad_time", "Network cleanup pending: %s", esp_err_to_name(rc));
}
void kp_network_tick(uint32_t now) {
    if (s_suspended) return;
    kp_network_status_t status = kp_network_status();
    if (status != KP_NET_CONNECTING && status != KP_NET_SYNCING) {
        if (stop() != ESP_OK) return;
        if (s_scheduled && (int32_t)(now - s_next_at) < 0) return;
        kp_wifi_config_t wifi; kp_store_wifi_get(&wifi);
        if (!wifi.ssid[0]) { atomic_store(&s_status, KP_NET_DISABLED); return; }
        if (start(&wifi, now) != ESP_OK) finish(now, false);
        return;
    }
    if (atomic_exchange(&s_synced, false)) {
        // SNTP has updated system UTC. Read it in the worker, not in a callback,
        // so delayed scheduling cannot pair an old epoch with a new anchor.
        uint64_t epoch = (uint64_t)time(NULL);
        if (kp_store_time_sync(epoch, (uint64_t)esp_timer_get_time() / 1000)) {
            ESP_LOGI("keypad_time", "Network time synchronized"); finish(now, true); return;
        }
    }
    if ((uint32_t)(now - s_attempt_at) >= ATTEMPT_MS) { finish(now, false); return; }
    if (atomic_load(&s_got_ip)) {
        if (!s_sntp) {
            esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3,
                ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "time.cloudflare.com", "pool.ntp.org"));
            config.sync_cb = time_synced; config.wait_for_sync = false;
            if (esp_netif_sntp_init(&config) != ESP_OK) { finish(now, false); return; }
            s_sntp = true; atomic_store(&s_status, KP_NET_SYNCING);
        }
    } else {
        if (s_sntp) { esp_netif_sntp_deinit(); s_sntp = false; }
        atomic_store(&s_status, KP_NET_CONNECTING);
        if (atomic_load(&s_reconnect) && s_connects < 3 && (uint32_t)(now - s_connect_at) >= CONNECT_RETRY_MS) {
            atomic_store(&s_reconnect, false); s_connect_at = now; ++s_connects;
            if (esp_wifi_connect() != ESP_OK) atomic_store(&s_reconnect, true);
        }
    }
}
