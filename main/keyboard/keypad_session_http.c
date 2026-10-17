#include "keypad_session_http.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

static esp_err_t fail(httpd_req_t *r, const char *status, const char *message) {
    httpd_resp_set_status(r, status);
    return httpd_resp_sendstr(r, message);
}

esp_err_t kp_session_http(httpd_req_t *r, const char *ip) {
    char host[40], expected[64], origin[64];
    snprintf(expected, sizeof(expected), "%s:80", ip);
    if (httpd_req_get_hdr_value_str(r, "Host", host, sizeof(host)) != ESP_OK ||
        (strcmp(host, ip) && strcmp(host, expected)))
        return fail(r, "403 Forbidden", "Use the Passport address");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_type(r, "application/json");
    if (r->method == HTTP_POST) {
        snprintf(expected, sizeof(expected), "http://%s", host);
        if (httpd_req_get_hdr_value_str(r, "Origin", origin, sizeof(origin)) != ESP_OK || strcmp(origin, expected))
            return fail(r, "403 Forbidden", "Same-origin requests only");
        if (!r->content_len || r->content_len > 80) return fail(r, "400 Bad Request", "Invalid mode");
        char body[81];
        size_t used = 0;
        while (used < r->content_len) {
            int n = httpd_req_recv(r, body + used, r->content_len - used);
            if (n <= 0) return fail(r, "400 Bad Request", "Incomplete request");
            used += (size_t)n;
        }
        body[used] = 0;
        const char *end = NULL;
        cJSON *json = cJSON_ParseWithLengthOpts(body, used + 1, &end, true);
        cJSON *field = cJSON_GetObjectItemCaseSensitive(json, "mode");
        bool resume = !strcmp(r->uri, "/api/session/resume");
        cJSON *epoch = cJSON_GetObjectItemCaseSensitive(json, "generation");
        uint32_t expected_generation = 0;
        if (cJSON_IsNumber(epoch) && epoch->valuedouble >= 1 && epoch->valuedouble <= UINT32_MAX &&
            epoch->valuedouble == (uint32_t)epoch->valuedouble)
            expected_generation = (uint32_t)epoch->valuedouble;
        kp_session_mode_t mode = KP_SESSION_OFF;
        if (end == body + used && cJSON_IsObject(json) && cJSON_GetArraySize(json) == (resume ? 2 : 1) && cJSON_IsString(field)) {
            if (!strcmp(field->valuestring, "transfer")) mode = KP_SESSION_TRANSFER;
            if (!strcmp(field->valuestring, "config")) mode = KP_SESSION_CONFIG;
        }
        cJSON_Delete(json);
        if (mode == KP_SESSION_OFF) return fail(r, "400 Bad Request", "Invalid mode");
        kp_session_view_t current;
        kp_session_get(&current);
        if (current.mode != mode) return fail(r, "409 Conflict", "Enable this function on Passport first");
        uint32_t generation = expected_generation;
        if (resume && !kp_session_current(mode, generation)) return fail(r, "409 Conflict", "Session ended");
        uint32_t id = resume ? kp_session_reserve(mode, generation, (uint64_t)(esp_timer_get_time() / 1000)) :
            kp_session_open(mode, (uint64_t)(esp_timer_get_time() / 1000), &generation);
        if (!id) return !kp_session_current(mode, generation) ?
            fail(r, "409 Conflict", "Session ended") : fail(r, "503 Service Unavailable", "Four pages maximum");
        char response[120];
        snprintf(response, sizeof(response), "{\"generation\":%lu,\"lease\":%lu}",
            (unsigned long)generation, (unsigned long)id);
        esp_err_t rc = httpd_resp_sendstr(r, response);
        if (rc != ESP_OK) kp_session_leave(mode, generation, id, (uint64_t)(esp_timer_get_time() / 1000));
        return rc;
    }
    kp_session_view_t v;
    kp_session_get(&v);
    char response[160];
    const char *mode = v.mode == KP_SESSION_TRANSFER ? "transfer" : v.mode == KP_SESSION_CONFIG ? "config" : "off";
    snprintf(response, sizeof(response), "{\"mode\":\"%s\",\"generation\":%lu,\"online\":%u,\"joining\":%u,\"ready\":%s}",
        mode, (unsigned long)v.generation, v.online, v.joining, v.ready ? "true" : "false");
    return httpd_resp_sendstr(r, response);
}
