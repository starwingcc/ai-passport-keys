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
static void web_request(bool forget) {
    if (forget) atomic_store(&s_forget, true);
    kp_network_service_ap(false);
}
static void memory_log(const char *phase) {
    ESP_LOGI("transfer", "%s free=%u largest=%u min=%u stack=%u", phase,
        (unsigned)esp_get_free_heap_size(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)esp_get_minimum_free_heap_size(), (unsigned)uxTaskGetStackHighWaterMark(NULL));
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
// A finite lifecycle step makes ownership and failed teardown testable. Only
// the network task calls this in firmware; event/HTTP/button callbacks enqueue.
static void step(service_t *s, uint32_t now) {
    kp_network_view_t *v = &s->view;
    unsigned request = atomic_exchange(&s_transfer_request, 0);
    unsigned ap = atomic_exchange(&s_ap_request, 0);
    if (ap) {
        s->ap_desired = ap == 1;
        if (s->ap_desired) s->desired = false;
    }
    if (request && !s->ap_desired && v->ap == KP_AP_OFF && v->transfer.state != KP_TRANSFER_STOPPING) {
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
    if (s->ap_desired && v->ap != KP_AP_ACTIVE && v->ap != KP_AP_FAILED) {
        v->ap = KP_AP_STARTING;
        v->transfer.state = KP_TRANSFER_OFF;
        publish(v);
        esp_err_t rc = kp_network_suspend();
        if (rc == ESP_OK) rc = kp_ble_pause(true);
        if (rc == ESP_OK) rc = kp_web_start(web_request);
        if (rc == ESP_OK) {
            v->ap = KP_AP_ACTIVE;
            snprintf(v->ssid, sizeof(v->ssid), "%s", kp_web_ssid());
            snprintf(v->password, sizeof(v->password), "%s", kp_web_password());
        } else {
            v->ap = KP_AP_FAILED;
            if (kp_web_stop() == ESP_OK) { kp_ble_pause(false); kp_network_resume(); }
        }
        publish(v);
        memory_log("AP start");
    }
    if (!s->ap_desired && v->ap != KP_AP_OFF) {
        v->ap = KP_AP_STOPPING;
        publish(v);
        if (kp_web_stop() == ESP_OK && kp_ble_pause(false) == ESP_OK) {
            kp_network_resume();
            v->ap = KP_AP_OFF;
            v->ssid[0] = v->password[0] = 0;
            v->forget_requested = atomic_exchange(&s_forget, false);
            publish(v);
            memory_log("AP stop");
        } else {
            v->ap = KP_AP_FAILED;
            publish(v);
            return;
        }
    }
    if (v->ap == KP_AP_ACTIVE) kp_wifi_setup_poll(now);
    else if (v->ap == KP_AP_OFF) {
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
