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
static atomic_bool s_forget;
static kp_network_view_t s_view;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;

typedef struct {
    kp_network_view_t view;
    bool desired, ap_desired, had_ready, server_failed;
    bool config_started, config_web_started;
    bool config_ble_paused;
    uint32_t config_at;
    uint32_t last_poll, last_stop_retry;
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
static void web_request(bool forget) {
    if (forget) atomic_store(&s_forget, true);
    kp_network_service_ap(false);
}
static void memory_log(const char *phase) {
    ESP_LOGI("transfer", "%s free=%u largest=%u min=%u stack=%u dma=%u dma_largest=%u", phase,
        (unsigned)esp_get_free_heap_size(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)esp_get_minimum_free_heap_size(), (unsigned)uxTaskGetStackHighWaterMark(NULL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
}
static esp_err_t stop_transfer(kp_network_view_t *view) {
    view->transfer.state = KP_TRANSFER_STOPPING;
    view->transfer.ip[0] = 0;
    view->transfer.peers = 0;
    publish(view);
    esp_err_t rc = kp_transfer_web_stop();
    if (rc != ESP_OK) return rc;
    kp_network_keep_online(false);
    rc = kp_network_suspend();
    if (rc != ESP_OK) return rc;
    view->transfer = (kp_transfer_view_t){.state = KP_TRANSFER_OFF};
    return ESP_OK;
}
static esp_err_t resume_config_ble(service_t *s) {
    if (!s->config_ble_paused) return ESP_OK;
    esp_err_t rc = kp_ble_pause(false);
    if (rc == ESP_OK) s->config_ble_paused = false;
    return rc;
}
// A finite lifecycle step makes ownership and failed teardown testable. Only
// the network task calls this in firmware; event/HTTP/button callbacks enqueue.
static void step(service_t *s, uint32_t now) {
    kp_network_view_t *v = &s->view;
    kp_wifi_config_t saved;
    kp_store_wifi_get(&saved);
    v->lan.configured = saved.ssid[0] != 0;
    // Saved edits apply after exit; show the network used by this session.
    if (v->lan.state == KP_AP_OFF) snprintf(v->lan.ssid, sizeof(v->lan.ssid), "%s", saved.ssid);
    memset(&saved, 0, sizeof(saved));
    unsigned request = atomic_exchange(&s_transfer_request, 0);
    unsigned ap = atomic_exchange(&s_ap_request, 0);
    if (ap) {
        s->ap_desired = ap != 2;
        if (s->ap_desired) {
            s->desired = false;
            if (v->lan.state == KP_AP_OFF) v->lan.wifi = ap == 3;
        }
    }
    if (request && !s->ap_desired && v->lan.state == KP_AP_OFF && v->transfer.state != KP_TRANSFER_STOPPING) {
        s->desired = request == 1;
        if (!s->desired && !v->transfer.enabled && !kp_transfer_web_running()) v->transfer = (kp_transfer_view_t){0};
        if (s->desired) {
            kp_wifi_config_t wifi;
            kp_store_wifi_get(&wifi);
            if (!wifi.ssid[0]) {
                s->desired = false;
                v->transfer = (kp_transfer_view_t){.state = KP_TRANSFER_NO_WIFI};
            } else {
                v->transfer.enabled = true;
                v->transfer.state = KP_TRANSFER_CONNECTING;
                s->server_failed = false;
                kp_network_keep_online(true);
            }
            memset(&wifi, 0, sizeof(wifi));
            publish(v);
        }
    }
    if (!s->desired && (v->transfer.enabled || kp_transfer_web_running())) {
        if ((uint32_t)(now - s->last_stop_retry) < 1000U && v->transfer.state == KP_TRANSFER_STOPPING) return;
        s->last_stop_retry = now;
        if (stop_transfer(v) != ESP_OK) return;
        s->had_ready = s->server_failed = false;
        if (!s->ap_desired) kp_network_resume();
        memory_log("STA transfer stopped");
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
        kp_network_tick(now);
        char ip[16];
        bool connected = kp_network_ip(ip);
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
            s->config_started = s->config_web_started = false;
            v->lan.state = KP_AP_OFF;
            v->ssid[0] = v->password[0] = 0;
            v->forget_requested = atomic_exchange(&s_forget, false);
            publish(v);
            memory_log("AP stop");
        } else {
            v->lan.state = KP_AP_FAILED;
            publish(v);
            return;
        }
    }
    if (v->lan.state == KP_AP_ACTIVE) {
        if ((uint32_t)(now - s->last_poll) >= 1000U) {
            kp_web_poll(now);
            s->last_poll = now;
        }
        if (!v->lan.wifi) kp_wifi_setup_poll(now);
    } else if (v->lan.state == KP_AP_OFF) {
        kp_network_tick(now);
        if (s->desired) {
            char ip[16];
            bool connected = kp_network_ip(ip);
            if (!connected || (v->transfer.ip[0] && strcmp(v->transfer.ip, ip))) {
                v->transfer.ip[0] = 0;
                v->transfer.peers = 0;
                v->transfer.state = kp_network_status() == KP_NET_FAILED ? KP_TRANSFER_FAILED :
                    s->had_ready ? KP_TRANSFER_RECONNECTING : KP_TRANSFER_CONNECTING;
                publish(v);
                if (kp_transfer_web_running() && kp_transfer_web_stop() != ESP_OK) return;
            }
            if (connected && !s->server_failed && !kp_transfer_web_ready()) {
                esp_err_t rc = kp_transfer_web_running() ? kp_transfer_web_stop() : ESP_OK;
                if (rc == ESP_OK) rc = kp_transfer_web_start(ip);
                if (rc != ESP_OK) {
                    s->server_failed = true;
                    v->transfer.state = KP_TRANSFER_FAILED;
                    v->transfer.ip[0] = 0;
                } else {
                    v->transfer.state = KP_TRANSFER_READY;
                    snprintf(v->transfer.ip, sizeof(v->transfer.ip), "%s", ip);
                    s->had_ready = true;
                    memory_log("STA transfer ready");
                }
            }
            if (s->server_failed) v->transfer.state = KP_TRANSFER_FAILED;
            if (kp_transfer_web_ready() && !s->server_failed && connected) {
                v->transfer.state = KP_TRANSFER_READY;
                snprintf(v->transfer.ip, sizeof(v->transfer.ip), "%s", ip);
                v->transfer.peers = kp_transfer_web_peers();
                if ((uint32_t)(now - s->last_poll) >= 1000U) {
                    kp_transfer_web_poll(now);
                    s->last_poll = now;
                }
            }
        }
    }
    publish(v);
}
static void worker(void *arg) {
    (void)arg;
    service_t state = {0};
    for (;;) {
        step(&state, (uint32_t)(esp_timer_get_time() / 1000));
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
esp_err_t kp_network_service_start(void) {
    if (s_task) return ESP_OK;
    return xTaskCreate(worker, "keypad_network", 4096, NULL, 3, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
