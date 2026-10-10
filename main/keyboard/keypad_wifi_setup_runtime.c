#include "keypad_wifi_setup_runtime.h"
#include "keypad_network.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <string.h>

enum { SCAN_DONE = 1, GOT_IP = 2, DISCONNECTED = 4 };
static SemaphoreHandle_t s_lock;
static kp_wifi_job_t s_job;
static kp_wifi_config_t s_candidate;
static bool s_ready, s_queued, s_scan, s_station, s_finishing;
static bool s_disconnect_requested;
static bool s_lan, s_borrowed, s_restore, s_restoring, s_leaving;
static bool s_address_borrowed;
static wifi_config_t s_original;
static kp_wifi_job_state_t s_final;
static uint32_t s_at, s_cleanup_at;
static esp_event_handler_instance_t s_wifi_handler, s_ip_handler;
static atomic_bool s_events_enabled;
static atomic_uint s_events, s_reason, s_scan_status;

static void event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (!atomic_load(&s_events_enabled)) return;
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        atomic_store(&s_scan_status, ((wifi_event_sta_scan_done_t *)data)->status);
        atomic_fetch_or(&s_events, SCAN_DONE);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&s_reason, ((wifi_event_sta_disconnected_t *)data)->reason);
        atomic_fetch_or(&s_events, DISCONNECTED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) atomic_fetch_or(&s_events, GOT_IP);
}
void kp_wifi_setup_get(kp_wifi_job_t *out) {
    if (!s_lock) { *out = (kp_wifi_job_t){0}; return; }
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_job; xSemaphoreGive(s_lock);
}
bool kp_wifi_setup_testing(void) {
    if (!s_lock) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool testing = s_job.busy && s_job.kind == KP_WIFI_TEST;
    xSemaphoreGive(s_lock); return testing;
}
esp_err_t kp_wifi_setup_start(bool lan) {
    if (s_wifi_handler || s_ip_handler || s_station || s_scan) return ESP_ERR_INVALID_STATE;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t previous_id = s_job.id; s_job = (kp_wifi_job_t){.id = previous_id};
    s_ready = false; s_queued = false; s_candidate = (kp_wifi_config_t){0};
    s_lan = lan;
    xSemaphoreGive(s_lock);
    atomic_store(&s_events, 0);
    esp_err_t rc = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL, &s_wifi_handler);
    if (rc == ESP_OK) rc = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, NULL, &s_ip_handler);
    if (rc != ESP_OK) return rc;
    atomic_store(&s_events_enabled, true);
    xSemaphoreTake(s_lock, portMAX_DELAY); s_ready = true; xSemaphoreGive(s_lock);
    return ESP_OK;
}
static esp_err_t queue(kp_wifi_job_kind_t kind, const kp_wifi_config_t *candidate, uint32_t *id) {
    if (!id) return ESP_ERR_INVALID_ARG;
    if (!s_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t rc = ESP_ERR_INVALID_STATE;
    if (s_ready && !s_job.busy) {
        uint32_t next = s_job.id + 1; if (!next) next = 1;
        s_job = (kp_wifi_job_t){.id = next, .kind = kind, .state = KP_WIFI_QUEUED, .busy = true};
        if (candidate) { s_candidate = *candidate; memcpy(s_job.ssid, candidate->ssid, sizeof(s_job.ssid)); }
        s_queued = true; *id = next; rc = ESP_OK;
    }
    xSemaphoreGive(s_lock); return rc;
}
esp_err_t kp_wifi_setup_scan(uint32_t *id) { return queue(KP_WIFI_SCAN, NULL, id); }
esp_err_t kp_wifi_setup_test(const kp_wifi_config_t *candidate, uint32_t *id) {
    if (!kp_wifi_valid(candidate) || !candidate->ssid[0]) return ESP_ERR_INVALID_ARG;
    return queue(KP_WIFI_TEST, candidate, id);
}
static esp_err_t clear_station(void) {
    wifi_config_t empty = {0};
    return esp_wifi_set_config(WIFI_IF_STA, &empty);
}
esp_err_t kp_wifi_setup_stop(void) {
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY); s_ready = false; s_queued = false;
        s_candidate = (kp_wifi_config_t){0}; xSemaphoreGive(s_lock);
    }
    atomic_store(&s_events_enabled, false);
    if (s_scan) {
        esp_err_t rc = esp_wifi_scan_stop();
        if (rc != ESP_OK) return rc;
        rc = esp_wifi_clear_ap_list(); if (rc != ESP_OK) return rc;
        s_scan = false;
    }
    if (s_station) {
        esp_err_t rc = esp_wifi_disconnect();
        if (rc != ESP_OK && rc != ESP_ERR_WIFI_NOT_CONNECT) return rc;
        if (s_address_borrowed) {
            rc = kp_network_setup_test_address(false);
            if (rc != ESP_OK) return rc;
            s_address_borrowed = false;
        }
        rc = s_restore ? esp_wifi_set_config(WIFI_IF_STA, &s_original) : clear_station();
        if (rc != ESP_OK) return rc;
        s_station = false;
    }
    // The caller tears down STA after exit; do not reconnect a cancelled test.
    s_restore = s_restoring = s_leaving = false;
    memset(&s_original, 0, sizeof(s_original));
    if (s_borrowed) { kp_network_setup_pause(false, (uint32_t)(esp_timer_get_time() / 1000)); s_borrowed = false; }
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
    s_finishing = false; s_disconnect_requested = false; atomic_store(&s_events, 0);
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY); s_job.busy = false; s_job.state = KP_WIFI_IDLE;
        xSemaphoreGive(s_lock);
    }
    return ESP_OK;
}
static void state(kp_wifi_job_state_t next, uint32_t now) {
    xSemaphoreTake(s_lock, portMAX_DELAY); s_job.state = next; xSemaphoreGive(s_lock); s_at = now;
}
static void finish(kp_wifi_job_state_t result, uint32_t now) {
    s_final = result; s_finishing = true; s_cleanup_at = now; s_disconnect_requested = false;
}
static bool matches(const uint8_t ssid[32], const char *name) {
    size_t length = 0; while (length < 32 && name[length]) ++length;
    return !memcmp(ssid, name, length) && (length == 32 || ssid[length] == 0);
}
static void complete(void) {
    atomic_store(&s_events, 0); s_finishing = false; s_restoring = false;
    s_restore = false; memset(&s_original, 0, sizeof(s_original));
    if (s_borrowed) { kp_network_setup_pause(false, (uint32_t)(esp_timer_get_time() / 1000)); s_borrowed = false; }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_candidate = (kp_wifi_config_t){0}; s_job.state = s_final; s_job.busy = false;
    xSemaphoreGive(s_lock);
}
static void cleanup(uint32_t now) {
    if (s_restoring) {
        unsigned events = atomic_exchange(&s_events, 0);
        wifi_ap_record_t ap;
        // GOT_IP alone can be stale; verify the restored association too.
        if ((events & GOT_IP) && esp_wifi_sta_get_ap_info(&ap) == ESP_OK &&
            matches(ap.ssid, (const char *)s_original.sta.ssid)) { complete(); return; }
        if ((events & DISCONNECTED) || kp_wifi_job_expired(KP_WIFI_RESTORING, (uint32_t)(now - s_cleanup_at))) {
            xSemaphoreTake(s_lock, portMAX_DELAY); s_job.restore_failed = true; xSemaphoreGive(s_lock);
            complete();
        }
        return;
    }
    if (s_scan) {
        if (esp_wifi_scan_stop() != ESP_OK || esp_wifi_clear_ap_list() != ESP_OK) return;
        s_scan = false;
    }
    if (s_station) {
        if (!s_disconnect_requested) {
            esp_err_t rc = esp_wifi_disconnect();
            if (rc != ESP_OK && rc != ESP_ERR_WIFI_NOT_CONNECT) return;
            s_disconnect_requested = true; s_cleanup_at = now;
        }
        // Drain disconnect/IP events before accepting another password test.
        wifi_ap_record_t ap;
        if ((uint32_t)(now - s_cleanup_at) < 100 || esp_wifi_sta_get_ap_info(&ap) == ESP_OK) return;
        if (s_address_borrowed) {
            if (kp_network_setup_test_address(false) != ESP_OK) return;
            s_address_borrowed = false;
        }
        if ((s_restore ? esp_wifi_set_config(WIFI_IF_STA, &s_original) : clear_station()) != ESP_OK) return;
        s_station = false;
    }
    if (s_restore) {
        atomic_store(&s_events, 0);
        s_restoring = true; s_cleanup_at = now; state(KP_WIFI_RESTORING, now);
        if (esp_wifi_connect() != ESP_OK) {
            xSemaphoreTake(s_lock, portMAX_DELAY); s_job.restore_failed = true; xSemaphoreGive(s_lock);
            complete();
        }
        return;
    }
    complete();
}
void kp_wifi_setup_poll(uint32_t now) {
    if (!s_lock) return;
    if (s_finishing) { cleanup(now); return; }
    kp_wifi_job_t view; kp_wifi_config_t candidate;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool queued = s_ready && s_queued;
    if (queued) s_queued = false;
    candidate = s_candidate;
    view = s_job; xSemaphoreGive(s_lock);
    if (!view.busy) return;
    if (queued) {
        atomic_store(&s_events, 0); atomic_store(&s_reason, 0);
        if (s_lan) { kp_network_setup_pause(true, now); s_borrowed = true; }
        if (view.kind == KP_WIFI_SCAN) {
            state(KP_WIFI_SCANNING, now);
            if (esp_wifi_scan_start(NULL, false) != ESP_OK) finish(KP_WIFI_ERROR, now);
            else s_scan = true;
        } else {
            state(KP_WIFI_CONNECTING, now);
            if (s_lan) {
                if (esp_wifi_get_config(WIFI_IF_STA, &s_original) != ESP_OK) { finish(KP_WIFI_ERROR, now); return; }
                s_restore = s_station = s_leaving = true; s_cleanup_at = now;
                esp_err_t rc = esp_wifi_disconnect();
                if (rc != ESP_OK && rc != ESP_ERR_WIFI_NOT_CONNECT) { s_leaving = false; finish(KP_WIFI_ERROR, now); }
                return;
            }
        }
        if (view.kind == KP_WIFI_SCAN) return;
    }
    if (s_leaving) {
        wifi_ap_record_t ap;
        if ((uint32_t)(now - s_cleanup_at) < 100 || esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            if (kp_wifi_job_expired(KP_WIFI_CONNECTING, (uint32_t)(now - s_cleanup_at))) { s_leaving = false; finish(KP_WIFI_TIMEOUT, now); }
            return;
        }
        s_leaving = false; atomic_store(&s_events, 0);
        state(KP_WIFI_CONNECTING, now);
        queued = true;
    }
    if (queued && view.kind == KP_WIFI_TEST) {
        wifi_config_t cfg = {0};
        memcpy(cfg.sta.ssid, candidate.ssid, strlen(candidate.ssid));
        memcpy(cfg.sta.password, candidate.password, strlen(candidate.password));
        cfg.sta.threshold.authmode = candidate.password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
        esp_err_t rc = ESP_OK;
        if (s_lan) {
            s_address_borrowed = true;
            rc = kp_network_setup_test_address(true);
        }
        if (rc == ESP_OK) rc = esp_wifi_set_config(WIFI_IF_STA, &cfg);
        xSemaphoreTake(s_lock, portMAX_DELAY); s_candidate = (kp_wifi_config_t){0}; xSemaphoreGive(s_lock);
        memset(&candidate, 0, sizeof(candidate)); memset(&cfg, 0, sizeof(cfg));
        if (rc == ESP_OK) { s_station = true; rc = esp_wifi_connect(); }
        if (rc != ESP_OK) finish(KP_WIFI_ERROR, now);
        return;
    }
    unsigned events = atomic_exchange(&s_events, 0);
    if (view.state == KP_WIFI_SCANNING && (events & SCAN_DONE)) {
        uint16_t total = 0;
        esp_err_t rc = atomic_load(&s_scan_status) ? ESP_FAIL : esp_wifi_scan_get_ap_num(&total);
        // One record at a time avoids a large temporary array. Bound processing
        // even in crowded locations, then explicitly release all remaining APs.
        for (unsigned i = 0; rc == ESP_OK && i < total && i < 64; ++i) {
            wifi_ap_record_t ap;
            rc = esp_wifi_scan_get_ap_record(&ap);
            if (rc == ESP_OK) kp_wifi_scan_add(&view, (const char *)ap.ssid, ap.rssi, ap.authmode != WIFI_AUTH_OPEN);
        }
        esp_err_t cleared = esp_wifi_clear_ap_list();
        if (cleared == ESP_OK) s_scan = false;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_job.count = view.count; memcpy(s_job.networks, view.networks, sizeof(view.networks));
        xSemaphoreGive(s_lock);
        finish(rc == ESP_OK && cleared == ESP_OK ? KP_WIFI_SCAN_DONE : KP_WIFI_ERROR, now); return;
    }
    if (view.kind == KP_WIFI_TEST && (events & DISCONNECTED)) {
        uint16_t reason = atomic_load(&s_reason);
        xSemaphoreTake(s_lock, portMAX_DELAY); s_job.reason = reason; xSemaphoreGive(s_lock);
        finish(kp_wifi_failure(reason), now); return;
    }
    if (view.state == KP_WIFI_CONNECTING && (events & GOT_IP)) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK && matches(ap.ssid, view.ssid)) {
            xSemaphoreTake(s_lock, portMAX_DELAY); s_job.connected = true; xSemaphoreGive(s_lock);
            finish(KP_WIFI_VERIFIED, now);
        }
        return;
    }
    if (kp_wifi_job_expired(view.state, (uint32_t)(now - s_at)))
        finish(KP_WIFI_TIMEOUT, now);
}
