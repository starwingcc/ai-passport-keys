#include "keypad_network_service.h"
#include "keypad_network.h"
#include "keypad_transfer_web.h"
#include "keypad_web.h"
#include "keypad_store.h"
#include "keypad_ble.h"
#include "keypad_wifi_setup_runtime.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static atomic_uint s_transfer_request, s_ap_request;
static atomic_bool s_forget, s_config_exit;
static bool s_wake_pending;
static uint64_t s_wake_at;
static kp_network_view_t s_view;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;

typedef struct {
    kp_network_view_t view;
    bool desired, ap_desired, had_ready;
    bool stop_requested, start_retry, stop_retry, link_down, disconnect_pending;
    kp_transfer_lifecycle_t lifecycle;
    uint64_t start_retry_at, stop_retry_at, background_poll_at;
    uint64_t config_handoff_at;
    unsigned pending_config;
    bool config_started, config_web_started, config_testing;
    bool config_ble_paused;
    uint32_t config_at;
    uint32_t last_poll;
} service_t;

static void publish(const kp_network_view_t *view) {
    portENTER_CRITICAL(&s_lock);
    s_view = *view;
    portEXIT_CRITICAL(&s_lock);
}
void kp_network_service_get(kp_network_view_t *view) {
    portENTER_CRITICAL(&s_lock);
    *view = s_view;
    portEXIT_CRITICAL(&s_lock);
}
void kp_network_service_transfer(bool enable) { atomic_store(&s_transfer_request, enable ? 1 : 2); }
void kp_network_service_ap(bool enable) { atomic_store(&s_ap_request, enable ? 1 : 2); }
void kp_network_service_config(bool enable, bool wifi) { atomic_store(&s_ap_request, enable ? (wifi ? 3 : 1) : 2); }
static void web_wake(uint64_t now) {
    portENTER_CRITICAL(&s_lock);
    if (!s_wake_pending) { s_wake_at = now; s_wake_pending = true; }
    portEXIT_CRITICAL(&s_lock);
}
static void config_wake(uint64_t now) {
    (void)now;
    unsigned empty = 0;
    // Browser visits must never overwrite a queued manual stop or AP request.
    atomic_compare_exchange_strong(&s_ap_request, &empty, 4);
}
static void web_request(bool forget) {
    if (forget) atomic_store(&s_forget, true);
    atomic_store(&s_config_exit, true);
}
static void memory_log(const char *phase) {
    ESP_LOGI("transfer", "%s free=%u largest=%u min=%u stack=%u dma=%u dma_largest=%u", phase,
        (unsigned)esp_get_free_heap_size(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)esp_get_minimum_free_heap_size(), (unsigned)uxTaskGetStackHighWaterMark(NULL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
}
static esp_err_t resume_config_ble(service_t *s) {
    if (!s->config_ble_paused) return ESP_OK;
    esp_err_t rc = kp_ble_pause(false);
    if (rc == ESP_OK) s->config_ble_paused = false;
    return rc;
}
// A finite lifecycle step makes ownership and failed teardown testable. Only
// the network task calls this in firmware; event/HTTP/button callbacks enqueue.
static void step(service_t *s, uint64_t monotonic_ms) {
    uint32_t now = (uint32_t)monotonic_ms; // Existing bounded Wi-Fi/config timers.
    kp_network_view_t *v = &s->view;
    kp_wifi_config_t saved;
    kp_store_wifi_get(&saved);
    v->lan.configured = saved.ssid[0] != 0;
    if (v->lan.state == KP_AP_OFF) snprintf(v->lan.ssid, sizeof(v->lan.ssid), "%s", saved.ssid);
    memset(&saved, 0, sizeof(saved));
    unsigned request = atomic_exchange(&s_transfer_request, 0);
    unsigned ap = atomic_exchange(&s_ap_request, 0);
    unsigned reopen = 0;
    if (atomic_exchange(&s_config_exit, false)) {
        // HTTP has already gated this session. Complete its stop even if a
        // simultaneous manual start arrived, then honor that new request.
        if (ap && ap != 2) reopen = ap;
        ap = 2;
    }
    uint64_t wake_at;
    bool wake;
    portENTER_CRITICAL(&s_lock);
    wake = s_wake_pending; wake_at = s_wake_at; s_wake_pending = false;
    portEXIT_CRITICAL(&s_lock);
    if (ap) {
        if (ap == 2) { s->ap_desired = false; s->pending_config = reopen; }
        else if (ap != 4 || !s->ap_desired) {
            if (v->lan.state != KP_AP_OFF && v->lan.wifi != (ap >= 3)) {
                s->pending_config = ap;
                s->ap_desired = false;
            } else {
                s->ap_desired = true;
                v->lan.wifi = ap >= 3;
                v->lan.automatic = ap == 4;
                // Let the small bootstrap response reach the browser before
                // stopping HTTP/STA. Its embedded script retries link loss.
                s->config_handoff_at = ap == 4 ? monotonic_ms + 300U : monotonic_ms;
            }
        }
    }
    if (!s->ap_desired && v->lan.state == KP_AP_OFF && v->transfer.state != KP_TRANSFER_STOPPING) {
        if (request == 2) s->stop_requested = true;
        else if (request == 1 || wake) {
            if (!v->lan.configured) v->transfer.state = KP_TRANSFER_NO_WIFI;
            else kp_transfer_lifecycle_enable(&s->lifecycle, wake ? wake_at : monotonic_ms);
        }
    }
    s->desired = s->lifecycle.phase != KP_TRANSFER_LISTEN;
    // Both listener and catalogs relinquish HTTP before configuration obtains
    // port 80 or the radio. A failed teardown retains the entire handoff lease.
    if (s->ap_desired && v->lan.state == KP_AP_OFF) {
        if (monotonic_ms < s->config_handoff_at) { publish(v); return; }
        v->transfer.state = KP_TRANSFER_STOPPING;
        v->transfer.ip[0] = 0; v->transfer.peers = 0;
        publish(v);
        if (s->stop_retry && monotonic_ms - s->stop_retry_at < 1000U) return;
        esp_err_t rc = kp_transfer_web_stop();
        if (rc == ESP_OK) rc = kp_network_suspend();
        if (rc != ESP_OK) { s->stop_retry = true; s->stop_retry_at = monotonic_ms; return; }
        kp_transfer_lifecycle_reset(&s->lifecycle);
        s->desired = s->stop_requested = s->stop_retry = s->start_retry = false;
        s->link_down = s->disconnect_pending = false;
        v->transfer = (kp_transfer_view_t){.state=KP_TRANSFER_OFF};
    }
    if (s->ap_desired && !s->config_started && v->lan.state == KP_AP_OFF) {
        v->lan.state = KP_AP_STARTING;
        v->transfer.state = KP_TRANSFER_OFF;
        publish(v);
        esp_err_t rc = kp_network_suspend();
        if (rc == ESP_OK && v->lan.wifi) {
            if (!v->lan.configured) v->lan.state = KP_AP_NO_WIFI;
            else {
                kp_network_keep_online(true);
                kp_network_resume();
                s->config_started = true;
                s->config_at = now;
            }
        } else if (rc == ESP_OK) {
            // The configuration SoftAP needs beacon/DHCP airtime. Preserve the
            // original AP handoff; STA configuration can keep BLE connected.
            // A timed-out pause may already have taken effect: retain cleanup
            // ownership until the matching resume succeeds.
            s->config_ble_paused = true;
            rc = kp_ble_pause(true);
            if (rc == ESP_OK) rc = kp_web_start(web_request);
            if (rc == ESP_OK) v->lan.state = KP_AP_ACTIVE;
        }
        if (rc == ESP_OK && !v->lan.wifi) {
            snprintf(v->ssid, sizeof(v->ssid), "%s", kp_web_ssid());
            snprintf(v->password, sizeof(v->password), "%s", kp_web_password());
        } else if (rc != ESP_OK) {
            v->lan.state = KP_AP_FAILED;
            if (kp_web_stop() == ESP_OK && resume_config_ble(s) == ESP_OK) kp_network_resume();
        }
        publish(v);
        memory_log("configuration start");
    }
    if (s->ap_desired && v->lan.wifi && s->config_started && v->lan.state != KP_AP_FAILED) {
        if (s->config_web_started) kp_wifi_setup_poll(now);
        bool testing = s->config_web_started && kp_wifi_setup_testing();
        if (testing) { s->config_testing = true; s->config_at = now; }
        // Keep HTTP and its session alive while the test temporarily owns STA.
        if (!testing) {
            kp_network_tick(now);
            char ip[16];
            bool connected = kp_network_ip(ip);
            if (connected && s->config_testing) {
                kp_web_update_ip(ip);
                snprintf(v->lan.ip, sizeof(v->lan.ip), "%s", ip);
                s->config_testing = false;
                publish(v);
            }
            if (!connected || (v->lan.ip[0] && strcmp(v->lan.ip, ip))) {
                if (v->lan.state == KP_AP_ACTIVE) s->config_at = now;
                v->lan.ip[0] = 0;
                v->lan.state = KP_AP_STARTING;
                publish(v);
                if (kp_web_stop() != ESP_OK) { v->lan.state = KP_AP_FAILED; publish(v); return; }
                s->config_web_started = false;
            }
            if (connected && !s->config_web_started) {
                if (kp_web_start_wifi(ip, web_request) != ESP_OK) v->lan.state = KP_AP_FAILED;
                else {
                    s->config_web_started = true;
                    v->lan.state = KP_AP_ACTIVE;
                    snprintf(v->lan.ip, sizeof(v->lan.ip), "%s", ip);
                }
            } else if (!connected && (kp_network_status() == KP_NET_FAILED || (uint32_t)(now - s->config_at) >= 60000U)) {
                v->lan.state = KP_AP_FAILED;
            }
            // Failed connection/HTTP startup must release STA, but never before HTTP.
            if (v->lan.state == KP_AP_FAILED && kp_web_stop() == ESP_OK) {
                kp_network_keep_online(false);
                if (kp_network_suspend() == ESP_OK) s->config_started = false;
            }
        }
    }
    if (!s->ap_desired && v->lan.state != KP_AP_OFF) {
        v->lan.state = KP_AP_STOPPING;
        v->lan.ip[0] = 0;
        publish(v);
        esp_err_t rc = kp_web_stop();
        if (rc == ESP_OK && v->lan.wifi) {
            kp_network_keep_online(false);
            rc = kp_network_suspend();
        }
        if (rc == ESP_OK) rc = resume_config_ble(s);
        if (rc == ESP_OK) {
            kp_network_resume();
            s->config_started = s->config_web_started = s->config_testing = false;
            v->lan.state = KP_AP_OFF;
            v->ssid[0] = v->password[0] = 0;
            v->forget_requested = atomic_exchange(&s_forget, false);
            if (s->pending_config) {
                unsigned empty = 0;
                atomic_compare_exchange_strong(&s_ap_request, &empty, s->pending_config);
                s->pending_config = 0;
            }
            publish(v);
            memory_log("AP stop");
        } else {
            v->lan.state = KP_AP_FAILED;
            publish(v);
            return;
        }
    }
    if (s->ap_desired && v->lan.automatic &&
        (v->lan.state == KP_AP_FAILED || v->lan.state == KP_AP_NO_WIFI))
        kp_network_service_ap(false);
    if (v->lan.state == KP_AP_ACTIVE) {
        if ((uint32_t)(now - s->last_poll) >= 1000U) {
            kp_web_poll(now);
            s->last_poll = now;
        }
        if (!v->lan.wifi) kp_wifi_setup_poll(now);
    } else if (v->lan.state == KP_AP_OFF) {
        kp_network_keep_online(v->lan.configured);
        kp_network_tick(now);
        char ip[16];
        bool connected = kp_network_ip(ip);
        if (!v->lan.configured) {
            if (kp_transfer_web_stop() != ESP_OK) { v->transfer.state=KP_TRANSFER_STOPPING; publish(v); return; }
            kp_transfer_lifecycle_reset(&s->lifecycle);
            s->desired = s->stop_requested = false;
            v->transfer.enabled = false; v->transfer.ip[0] = 0; v->transfer.peers = 0;
            if (v->transfer.state != KP_TRANSFER_NO_WIFI) v->transfer.state=KP_TRANSFER_OFF;
            publish(v); return;
        }
        if ((!connected && !s->link_down && kp_transfer_web_running()) || s->disconnect_pending) {
            s->disconnect_pending = true;
            if (s->stop_retry && monotonic_ms - s->stop_retry_at < 1000U) { publish(v); return; }
            if (kp_transfer_web_disconnect() != ESP_OK) {
                s->stop_retry = true; s->stop_retry_at = monotonic_ms;
                v->transfer.ip[0] = 0; v->transfer.peers = 0; v->transfer.state=KP_TRANSFER_RECONNECTING;
                publish(v); return;
            }
            s->disconnect_pending = false; s->link_down = true; s->stop_retry = false;
        }
        kp_transfer_presence_t presence;
        kp_transfer_web_presence(&presence);
        kp_transfer_lifecycle_observe(&s->lifecycle, &presence);
        bool expired = kp_transfer_lifecycle_expired(&s->lifecycle, monotonic_ms);
        if (s->stop_requested || expired || s->stop_retry) {
            if (s->stop_retry && monotonic_ms - s->stop_retry_at < 1000U) { publish(v); return; }
            esp_err_t rc = kp_transfer_web_deactivate(s->stop_requested);
            if (rc == ESP_ERR_INVALID_STATE && !s->stop_requested) {
                // The HTTP thread observed a browser joining before its gate.
                kp_transfer_web_presence(&presence);
                kp_transfer_lifecycle_observe(&s->lifecycle, &presence);
                s->stop_retry = false;
            } else if (rc != ESP_OK) {
                s->stop_retry = true; s->stop_retry_at = monotonic_ms;
                v->transfer.state=KP_TRANSFER_STOPPING; publish(v); return;
            } else {
                kp_transfer_lifecycle_reset(&s->lifecycle);
                s->stop_requested = s->stop_retry = s->start_retry = false;
                memory_log("listening standby");
            }
        }
        s->desired = s->lifecycle.phase != KP_TRANSFER_LISTEN;
        v->transfer.enabled = s->desired;
        if (connected && (!s->start_retry || monotonic_ms - s->start_retry_at >= 1000U)) {
            esp_err_t rc = ESP_OK;
            if (!kp_transfer_web_listening()) {
                rc = kp_transfer_web_listen_start(ip, web_wake, config_wake);
                if (rc == ESP_ERR_INVALID_STATE) {
                    // Includes partially registered HTTP startup and failed
                    // httpd_stop: reuse only a completely initialized instance.
                    rc = kp_transfer_web_stop();
                    if (rc == ESP_OK) rc = kp_transfer_web_listen_start(ip, web_wake, config_wake);
                }
            }
            if (rc == ESP_OK) s->link_down = false;
            if (rc == ESP_OK && s->desired && !kp_transfer_web_ready())
                rc = kp_transfer_web_activate(s->lifecycle.zero_since_ms);
            s->start_retry = rc != ESP_OK;
            if (s->start_retry) s->start_retry_at = monotonic_ms;
            else if (s->desired && !s->had_ready) { s->had_ready = true; memory_log("transfer ready"); }
        }
        if (kp_transfer_web_running() && monotonic_ms - s->background_poll_at >= 1000U) {
            kp_transfer_web_poll(monotonic_ms); s->background_poll_at = monotonic_ms;
        }
        kp_transfer_web_presence(&presence);
        kp_transfer_lifecycle_observe(&s->lifecycle, &presence);
        v->transfer.peers = connected ? presence.online : 0;
        v->transfer.ip[0] = 0;
        if (!connected) {
            v->transfer.state = kp_network_status()==KP_NET_FAILED ? KP_TRANSFER_FAILED :
                s->had_ready ? KP_TRANSFER_RECONNECTING : KP_TRANSFER_CONNECTING;
        } else if (s->start_retry || !kp_transfer_web_listening()) v->transfer.state=KP_TRANSFER_FAILED;
        else {
            snprintf(v->transfer.ip, sizeof(v->transfer.ip), "%s", ip);
            v->transfer.state = !s->desired ? KP_TRANSFER_LISTENING :
                kp_transfer_web_ready() ? (presence.online ? KP_TRANSFER_READY : KP_TRANSFER_WAITING) : KP_TRANSFER_CONNECTING;
        }
    }
    publish(v);
}
static void worker(void *arg) {
    (void)arg;
    service_t state = {0};
    for (;;) {
        step(&state, (uint64_t)(esp_timer_get_time() / 1000));
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
esp_err_t kp_network_service_start(void) {
    if (s_task) return ESP_OK;
    return xTaskCreate(worker, "keypad_network", 4096, NULL, 3, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
