#include "keypad_web.h"
#include "keypad_store.h"
#include "keypad_config.h"
#include "keypad_network.h"
#include "keypad_wifi_setup_runtime.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static httpd_handle_t s_http;
static esp_netif_t *s_netif, *s_station_netif;
static bool s_wifi_initialized, s_wifi_started;
static char s_ssid[32], s_session[33];
static const char s_password[] = "12345678";
#define ASSET_CHUNK_BYTES 512U
static kp_web_request_t s_request;
extern const char index_start[] __asm__("_binary_index_html_start");
extern const char index_end[] __asm__("_binary_index_html_end");
extern const char js_start[] __asm__("_binary_app_js_start");
extern const char js_end[] __asm__("_binary_app_js_end");
extern const char charset_start[] __asm__("_binary_charset_js_start");
extern const char charset_end[] __asm__("_binary_charset_js_end");
extern const char css_start[] __asm__("_binary_app_css_start");
extern const char css_end[] __asm__("_binary_app_css_end");

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
    else if (!strcmp(req->uri, "/charset.js")) { start = charset_start; end = charset_end; type = "text/javascript; charset=utf-8"; }
    else if (!strcmp(req->uri, "/app.css")) { start = css_start; end = css_end; type = "text/css; charset=utf-8"; }
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'");
    // Large single sends can exhaust TCP/Wi-Fi buffers on this no-PSRAM board.
    size_t remaining = end - start - 1;
    while (remaining) {
        size_t count = remaining < ASSET_CHUNK_BYTES ? remaining : ASSET_CHUNK_BYTES;
        esp_err_t rc = httpd_resp_send_chunk(req, start, count);
        if (rc != ESP_OK) {
            ESP_LOGW("keypad_web", "Asset send failed uri=%s free=%u largest=%u",
                     req->uri, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            return rc;
        }
        start += count; remaining -= count;
    }
    return httpd_resp_send_chunk(req, NULL, 0);
}
static bool authorized(httpd_req_t *req) {
    char token[sizeof(s_session)];
    if (httpd_req_get_hdr_value_len(req, "X-Session") != strlen(s_session)) return false;
    if (httpd_req_get_hdr_value_str(req, "X-Session", token, sizeof(token)) != ESP_OK) return false;
    return s_session[0] && strcmp(token, s_session) == 0;
}
typedef struct { httpd_req_t *request; size_t remaining; } body_reader_t;
static int read_body(void *user, char *buffer, size_t capacity) {
    body_reader_t *r = user;
    if (!r->remaining) return 0;
    if (capacity > r->remaining) capacity = r->remaining;
    int count = httpd_req_recv(r->request, buffer, capacity);
    if (count <= 0) return -1;
    r->remaining -= count; return count;
}
static bool write_body(void *user, const char *buffer, size_t size) {
    return httpd_resp_send_chunk(user, buffer, size) == ESP_OK;
}
static bool write_config(const kp_config_t *config, void *user) {
    return kp_config_encode(config, write_body, user);
}
static bool write_settings(const kp_settings_t *settings, void *user) {
    return kp_settings_encode(settings, write_body, user);
}
static esp_err_t api(httpd_req_t *req) {
    char host[32];
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK ||
        (strcmp(host, "192.168.4.1") && strcmp(host, "192.168.4.1:80")))
        return fail(req, "403 Forbidden", "请使用设备显示的地址打开配置页");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    if (req->method == HTTP_GET) {
        if (!strcmp(req->uri, "/api/settings")) {
            if (!authorized(req)) return fail(req, "403 Forbidden", "设置会话已失效，请刷新页面");
            if (!kp_store_settings_read(write_settings, req)) return ESP_FAIL;
            return httpd_resp_send_chunk(req, NULL, 0);
        }
        if (!strcmp(req->uri, "/api/wifi/job")) {
            if (!authorized(req)) return fail(req, "403 Forbidden", "设置会话已失效，请刷新页面");
            kp_wifi_job_t job; kp_wifi_setup_get(&job);
            if (!kp_wifi_job_encode(&job, write_body, req)) return ESP_FAIL;
            return httpd_resp_send_chunk(req, NULL, 0);
        }
        if (!strcmp(req->uri, "/api/wifi")) {
            if (!authorized(req)) return fail(req, "403 Forbidden", "设置会话已失效，请刷新页面");
            kp_wifi_config_t wifi; kp_store_wifi_get(&wifi);
            bool clock_valid = kp_store_time_read((uint64_t)esp_timer_get_time() / 1000) != 0;
            if (!kp_wifi_encode(&wifi, kp_network_status(), clock_valid, write_body, req)) return ESP_FAIL;
            return httpd_resp_send_chunk(req, NULL, 0);
        }
        if (!strcmp(req->uri, "/api/home")) {
            kp_home_config_t home; kp_store_home_get(&home);
            if (!kp_home_encode(&home, write_body, req)) return ESP_FAIL;
            return httpd_resp_send_chunk(req, NULL, 0);
        }
        esp_err_t rc = httpd_resp_send_chunk(req, "{\"config\":", 10);
        if (rc == ESP_OK && !kp_store_read(write_config, req)) rc = ESP_FAIL;
        if (rc != ESP_OK) return rc;
        char tail[64]; snprintf(tail, sizeof(tail), ",\"session\":\"%s\"}", s_session);
        rc = httpd_resp_send_chunk(req, tail, strlen(tail));
        return rc == ESP_OK ? httpd_resp_send_chunk(req, NULL, 0) : rc;
    }
    if (!authorized(req)) return fail(req, "403 Forbidden", "设置会话已失效，请刷新页面");
    size_t limit = !strcmp(req->uri, "/api/settings") ? KP_SETTINGS_JSON_BYTES : KP_JSON_BYTES;
    if (req->content_len > limit) return fail(req, "413 Payload Too Large", "配置过大");
    if (!strcmp(req->uri, "/api/wifi/scan") || !strcmp(req->uri, "/api/wifi/test")) {
        uint32_t id; esp_err_t rc;
        if (!strcmp(req->uri, "/api/wifi/scan")) {
            if (req->content_len) return fail(req, "400 Bad Request", "扫描不接受请求体");
            rc = kp_wifi_setup_scan(&id);
        } else {
            if (!req->content_len || req->content_len > KP_HOME_JSON_BYTES)
                return fail(req, "400 Bad Request", "Wi-Fi 配置大小无效");
            kp_wifi_config_t saved, candidate; kp_store_wifi_get(&saved);
            body_reader_t reader = {.request = req, .remaining = req->content_len};
            if (!kp_wifi_decode(read_body, &reader, &saved, &candidate) || reader.remaining || !candidate.ssid[0])
                return fail(req, "400 Bad Request", "请填写要测试的网络名称与密码");
            rc = kp_wifi_setup_test(&candidate, &id); memset(&candidate, 0, sizeof(candidate));
        }
        if (rc == ESP_ERR_INVALID_STATE) return fail(req, "409 Conflict", "正在扫描或测试，请稍后重试");
        if (rc != ESP_OK) return fail(req, "500 Internal Server Error", "无法开始扫描或连接测试");
        char response[40]; snprintf(response, sizeof(response), "{\"id\":%lu}", (unsigned long)id);
        return httpd_resp_sendstr(req, response);
    }
    if (!strcmp(req->uri, "/api/wifi")) {
        if (!req->content_len || req->content_len > KP_HOME_JSON_BYTES)
            return fail(req, "400 Bad Request", "Wi-Fi 配置大小无效");
        kp_wifi_config_t saved, wifi; kp_store_wifi_get(&saved);
        body_reader_t reader = {.request = req, .remaining = req->content_len};
        if (!kp_wifi_decode(read_body, &reader, &saved, &wifi) || reader.remaining)
            return fail(req, "400 Bad Request", "Wi-Fi 配置无效，请检查网络名称与密码");
        if (kp_store_wifi_save(&wifi) != ESP_OK)
            return fail(req, "500 Internal Server Error", "Wi-Fi 保存失败，仍使用原设置");
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    if (!strcmp(req->uri, "/api/home") || !strcmp(req->uri, "/api/time")) {
        if (!req->content_len || req->content_len > KP_HOME_JSON_BYTES)
            return fail(req, "400 Bad Request", "工牌或时间配置大小无效");
        body_reader_t reader = {.request = req, .remaining = req->content_len};
        if (!strcmp(req->uri, "/api/home")) {
            kp_home_config_t home;
            if (!kp_home_decode(read_body, &reader, &home) || reader.remaining)
                return fail(req, "400 Bad Request", "工牌配置无效，请检查名称、头像和时区");
            if (kp_store_home_save(&home) != ESP_OK)
                return fail(req, "500 Internal Server Error", "工牌保存失败，仍使用原设置");
        } else {
            uint64_t epoch;
            if (!kp_time_decode(read_body, &reader, &epoch) || reader.remaining ||
                !kp_store_time_sync(epoch, (uint64_t)esp_timer_get_time() / 1000))
                return fail(req, "400 Bad Request", "时间无效，请检查手机或电脑的系统时间");
        }
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    if (!strcmp(req->uri, "/api/settings")) {
        if (!req->content_len) return fail(req, "400 Bad Request", "配置为空");
        kp_settings_t *settings = malloc(sizeof(*settings));
        if (!settings) return fail(req, "500 Internal Server Error", "内存不足");
        body_reader_t reader = {.request = req, .remaining = req->content_len};
        bool valid = kp_settings_decode(read_body, &reader, settings) && !reader.remaining;
        if (!valid) { free(settings); return fail(req, "400 Bad Request", "完整配置无效，请检查工牌、Wi-Fi 和按键情景"); }
        esp_err_t saved = kp_store_settings_save(settings); free(settings);
        if (saved != ESP_OK) return fail(req, "500 Internal Server Error", "完整配置保存失败，请重试");
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    if (!strcmp(req->uri, "/api/config")) {
        if (!req->content_len) return fail(req, "400 Bad Request", "配置为空");
        kp_config_t *config = malloc(sizeof(*config));
        if (!config) {
            ESP_LOGE("keypad_web", "Config allocation failed free=%u largest=%u needed=%u",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                     (unsigned)sizeof(*config));
            return fail(req, "500 Internal Server Error", "内存不足");
        }
        body_reader_t reader = {.request = req, .remaining = req->content_len};
        bool valid = kp_config_decode(read_body, &reader, config) && !reader.remaining;
        if (!valid) { free(config); return fail(req, "400 Bad Request", "无效配置：请检查文字、步骤、情景或容量；OK 长按不可映射"); }
        esp_err_t saved = kp_store_save(config); free(config);
        if (saved != ESP_OK) return fail(req, "500 Internal Server Error", "保存失败，仍使用原配置");
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
    esp_err_t stopped = kp_wifi_setup_stop();
    if (stopped != ESP_OK) return stopped;
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
    if (s_station_netif) { esp_netif_destroy_default_wifi(s_station_netif); s_station_netif = NULL; }
    return ESP_OK;
}
esp_err_t kp_web_start(kp_web_request_t callback) {
    if (s_http) return ESP_OK;
    esp_err_t rc = kp_web_stop();
    if (rc != ESP_OK) return rc;
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ssid, sizeof(s_ssid), "Passport-Keys-%02X%02X", mac[4], mac[5]);
    for (unsigned i = 0; i < 16; ++i) snprintf(s_session + 2 * i, 3, "%02x", (unsigned)(esp_random() & 255));
    s_request = callback;
    s_netif = esp_netif_create_default_wifi_ap();
    if (!s_netif) return ESP_ERR_NO_MEM;
    s_station_netif = esp_netif_create_default_wifi_sta();
    if (!s_station_netif) { rc = ESP_ERR_NO_MEM; goto fail; }
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
    if (rc == ESP_OK) rc = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (rc == ESP_OK) rc = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    wifi_config_t station = {0};
    if (rc == ESP_OK) rc = esp_wifi_set_config(WIFI_IF_STA, &station);
    if (rc == ESP_OK) rc = kp_wifi_setup_start();
    if (rc == ESP_OK) rc = esp_wifi_start();
    if (rc != ESP_OK) goto fail;
    s_wifi_started = true;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.max_open_sockets = 3; http.max_uri_handlers = 18;
    http.lru_purge_enable = true; http.stack_size = 6144;
    http.recv_wait_timeout = 5; http.send_wait_timeout = 5;
    rc = httpd_start(&s_http, &http);
    if (rc != ESP_OK) goto fail;
    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=static_file},
        {.uri="/app.js", .method=HTTP_GET, .handler=static_file},
        {.uri="/app.css", .method=HTTP_GET, .handler=static_file},
        {.uri="/charset.js", .method=HTTP_GET, .handler=static_file},
        {.uri="/api/config", .method=HTTP_GET, .handler=api},
        {.uri="/api/config", .method=HTTP_POST, .handler=api},
        {.uri="/api/settings", .method=HTTP_GET, .handler=api},
        {.uri="/api/settings", .method=HTTP_POST, .handler=api},
        {.uri="/api/home", .method=HTTP_GET, .handler=api},
        {.uri="/api/home", .method=HTTP_POST, .handler=api},
        {.uri="/api/time", .method=HTTP_POST, .handler=api},
        {.uri="/api/wifi", .method=HTTP_GET, .handler=api},
        {.uri="/api/wifi", .method=HTTP_POST, .handler=api},
        {.uri="/api/wifi/job", .method=HTTP_GET, .handler=api},
        {.uri="/api/wifi/scan", .method=HTTP_POST, .handler=api},
        {.uri="/api/wifi/test", .method=HTTP_POST, .handler=api},
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
