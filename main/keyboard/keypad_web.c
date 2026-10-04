#include "keypad_web.h"
#include "keypad_store.h"
#include "keypad_config.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_mac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static httpd_handle_t s_http;
static esp_netif_t *s_netif;
static bool s_wifi_initialized, s_wifi_started;
static char s_ssid[32], s_password[13], s_session[33];
static kp_web_request_t s_request;
extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
extern const char js_start[] asm("_binary_app_js_start");
extern const char js_end[] asm("_binary_app_js_end");
extern const char css_start[] asm("_binary_app_css_start");
extern const char css_end[] asm("_binary_app_css_end");

const char *kp_web_ssid(void) { return s_ssid; }
const char *kp_web_password(void) { return s_password; }
static esp_err_t fail(httpd_req_t *req, const char *status, const char *message) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(req, message);
}
static esp_err_t static_file(httpd_req_t *req) {
    const char *start = index_start, *end = index_end, *type = "text/html; charset=utf-8";
    if (!strcmp(req->uri, "/app.js")) { start = js_start; end = js_end; type = "text/javascript; charset=utf-8"; }
    else if (!strcmp(req->uri, "/app.css")) { start = css_start; end = css_end; type = "text/css; charset=utf-8"; }
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'");
    return httpd_resp_send(req, start, end - start - 1);
}
static bool authorized(httpd_req_t *req) {
    char token[sizeof(s_session)];
    if (httpd_req_get_hdr_value_len(req, "X-Session") != strlen(s_session)) return false;
    if (httpd_req_get_hdr_value_str(req, "X-Session", token, sizeof(token)) != ESP_OK) return false;
    return s_session[0] && strcmp(token, s_session) == 0;
}
static esp_err_t api(httpd_req_t *req) {
    char host[32];
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK ||
        (strcmp(host, "192.168.4.1") && strcmp(host, "192.168.4.1:80")))
        return fail(req, "403 Forbidden", "请使用设备显示的地址打开配置页");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    if (req->method == HTTP_GET) {
        kp_config_t config; kp_store_get(&config);
        cJSON *root = cJSON_CreateObject(), *body = kp_config_json(&config);
        if (!root || !body) { cJSON_Delete(root); cJSON_Delete(body); return fail(req, "500 Internal Server Error", "内存不足"); }
        if (!cJSON_AddItemToObject(root, "config", body)) {
            cJSON_Delete(body); cJSON_Delete(root); return fail(req, "500 Internal Server Error", "内存不足");
        }
        if (!cJSON_AddStringToObject(root, "session", s_session)) { cJSON_Delete(root); return fail(req, "500 Internal Server Error", "内存不足"); }
        char *text = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (!text) return fail(req, "500 Internal Server Error", "内存不足");
        esp_err_t rc = httpd_resp_sendstr(req, text);
        cJSON_free(text);
        return rc;
    }
    if (!authorized(req)) return fail(req, "403 Forbidden", "设置会话已失效，请刷新页面");
    if (req->content_len > 2048) return fail(req, "413 Payload Too Large", "配置过大");
    if (!strcmp(req->uri, "/api/config")) {
        if (!req->content_len) return fail(req, "400 Bad Request", "配置为空");
        char *body = malloc(req->content_len + 1);
        if (!body) return fail(req, "500 Internal Server Error", "内存不足");
        size_t used = 0;
        while (used < req->content_len) {
            int received = httpd_req_recv(req, body + used, req->content_len - used);
            if (received <= 0) { free(body); return ESP_FAIL; }
            used += received;
        }
        body[used] = 0;
        const char *end = NULL;
        cJSON *json = cJSON_ParseWithLengthOpts(body, used + 1, &end, true);
        kp_config_t config;
        bool valid = end == body + used && kp_config_parse(json, &config);
        cJSON_Delete(json); free(body);
        if (!valid) return fail(req, "400 Bad Request", "无效配置；OK 长按固定返回，不可映射");
        if (kp_store_save(&config) != ESP_OK) return fail(req, "500 Internal Server Error", "保存失败，仍使用原配置");
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    if (req->content_len) return fail(req, "400 Bad Request", "此操作不接受请求体");
    bool forget = !strcmp(req->uri, "/api/forget");
    esp_err_t rc = httpd_resp_sendstr(req, "{\"ok\":true}");
    if (rc == ESP_OK && s_request) s_request(forget);
    return rc;
}
esp_err_t kp_web_stop(void) {
    if (s_http) {
        esp_err_t rc = httpd_stop(s_http);
        if (rc != ESP_OK) return rc;
        s_http = NULL;
    }
    s_session[0] = 0;
    if (s_wifi_started) {
        esp_err_t rc = esp_wifi_stop();
        if (rc != ESP_OK) return rc;
        s_wifi_started = false;
    }
    if (s_wifi_initialized) {
        esp_err_t rc = esp_wifi_deinit();
        if (rc != ESP_OK) return rc;
        s_wifi_initialized = false;
    }
    if (s_netif) { esp_netif_destroy_default_wifi(s_netif); s_netif = NULL; }
    return ESP_OK;
}
esp_err_t kp_web_start(kp_web_request_t callback) {
    if (s_http) return ESP_OK;
    esp_err_t rc = kp_web_stop();
    if (rc != ESP_OK) return rc;
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ssid, sizeof(s_ssid), "Passport-Keys-%02X%02X", mac[4], mac[5]);
    const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    for (unsigned i = 0; i < sizeof(s_password) - 1; ++i) s_password[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
    for (unsigned i = 0; i < 16; ++i) snprintf(s_session + 2 * i, 3, "%02x", (unsigned)(esp_random() & 255));
    s_request = callback;
    s_netif = esp_netif_create_default_wifi_ap();
    if (!s_netif) return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    rc = esp_wifi_init(&init);
    if (rc != ESP_OK) goto fail;
    s_wifi_initialized = true;
    wifi_config_t cfg = {0};
    snprintf((char *)cfg.ap.ssid, sizeof(cfg.ap.ssid), "%s", s_ssid);
    snprintf((char *)cfg.ap.password, sizeof(cfg.ap.password), "%s", s_password);
    cfg.ap.ssid_len = strlen(s_ssid); cfg.ap.channel = 6; cfg.ap.max_connection = 1;
    cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    rc = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (rc == ESP_OK) rc = esp_wifi_set_mode(WIFI_MODE_AP);
    if (rc == ESP_OK) rc = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (rc == ESP_OK) rc = esp_wifi_start();
    if (rc != ESP_OK) goto fail;
    s_wifi_started = true;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.max_open_sockets = 3; http.max_uri_handlers = 8;
    http.lru_purge_enable = true; http.stack_size = 6144;
    http.recv_wait_timeout = 5; http.send_wait_timeout = 5;
    rc = httpd_start(&s_http, &http);
    if (rc != ESP_OK) goto fail;
    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=static_file},
        {.uri="/app.js", .method=HTTP_GET, .handler=static_file},
        {.uri="/app.css", .method=HTTP_GET, .handler=static_file},
        {.uri="/api/config", .method=HTTP_GET, .handler=api},
        {.uri="/api/config", .method=HTTP_POST, .handler=api},
        {.uri="/api/exit", .method=HTTP_POST, .handler=api},
        {.uri="/api/forget", .method=HTTP_POST, .handler=api},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        rc = httpd_register_uri_handler(s_http, &routes[i]);
        if (rc != ESP_OK) goto fail;
    }
    return ESP_OK;
fail:
    kp_web_stop();
    return rc;
}
