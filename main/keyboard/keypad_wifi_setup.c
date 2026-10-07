#include "keypad_wifi_setup.h"
#include "keypad_model.h"
#include <string.h>

void kp_wifi_scan_add(kp_wifi_job_t *job, const char *ssid, int rssi, bool secured) {
    if (!ssid || job->count > KP_WIFI_SCAN_RESULTS) return;
    size_t length = 0; while (length < 33 && ssid[length]) ++length;
    if (!length || length == 33 || !kp_text_utf8_valid(ssid, 32, false)) return;
    if (rssi < -127) rssi = -127;
    if (rssi > 127) rssi = 127;
    unsigned slot = job->count;
    for (unsigned i = 0; i < job->count; ++i) if (!strcmp(job->networks[i].ssid, ssid)) {
        if (job->networks[i].rssi >= rssi) return;
        slot = i; break;
    }
    if (slot == job->count) {
        if (slot == KP_WIFI_SCAN_RESULTS) {
            slot--;
            if (job->networks[slot].rssi >= rssi) return;
        } else ++job->count;
    }
    kp_wifi_scan_result_t result = {.rssi = rssi, .secured = secured};
    memcpy(result.ssid, ssid, strlen(ssid) + 1); job->networks[slot] = result;
    while (slot && job->networks[slot-1].rssi < result.rssi) {
        job->networks[slot] = job->networks[slot-1]; job->networks[--slot] = result;
    }
}
kp_wifi_job_state_t kp_wifi_failure(uint16_t reason) {
    switch (reason) {
    case 15: case 23: case 202: case 204: return KP_WIFI_AUTH_FAILED;
    case 201: case 212: return KP_WIFI_NOT_FOUND;
    case 210: case 211: return KP_WIFI_UNSUPPORTED;
    case 2: case 4: return KP_WIFI_TIMEOUT;
    default: return KP_WIFI_ERROR;
    }
}
bool kp_wifi_job_expired(kp_wifi_job_state_t state, uint32_t elapsed) {
    uint32_t limit = state == KP_WIFI_CONNECTING || state == KP_WIFI_RESTORING ? 30000U :
                     15000U;
    return elapsed >= limit;
}
