#include "keypad_web.h"
#include "keypad_session_http.h"
#include "keypad_web_asset.h"
#include "keypad_socket_io.h"
#include "keypad_store.h"
#include "keypad_config.h"
#include "keypad_network.h"
#include "keypad_wifi_setup_runtime.h"
#include "keypad_web_presence.h"
#include "cJSON.h"
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
#include <stdatomic.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

static httpd_handle_t s_http;
static esp_netif_t *s_netif, *s_station_netif;
static bool s_wifi_initialized, s_wifi_started;
static bool s_lan_wifi, s_setup_started;
static atomic_uint s_host_ip = 0x0104a8c0U;
static char s_ssid[32], s_session[33];
static const char s_password[] = "12345678";
static kp_web_request_t s_request;
static kp_web_presence_t s_presence;
static uint32_t s_generation;
#define HTTP_SOCKETS 3
#define HTTP_IDLE_MS 2000U
#define HTTP_SEND_STALL_MS 5000U
#define HTTP_SEND_TOTAL_MS 30000U
typedef struct {
    int fd;
    uint32_t used_at;
    bool used, handling, sending, failed;
    kp_socket_io_t io;
} client_t;
static client_t s_clients[HTTP_SOCKETS];
static atomic_bool s_stopping, s_poll_queued;
extern const char index_start[] __asm__("_binary_index_html_start");
extern const char index_end[] __asm__("_binary_index_html_end");
extern const char js_start[] __asm__("_binary_app_js_start");
extern const char js_end[] __asm__("_binary_app_js_end");
extern const char charset_start[] __asm__("_binary_charset_js_start");
extern const char charset_end[] __asm__("_binary_charset_js_end");
extern const char css_start[] __asm__("_binary_app_css_start");
extern const char css_end[] __asm__("_binary_app_css_end");

uint32_t kp_web_generation(void) { return s_generation; }
const char *kp_web_ssid(void) { return s_ssid; }
const char *kp_web_password(void) { return s_password; }
static uint32_t web_now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static client_t *client(int fd) {
    for (unsigned i = 0; i < HTTP_SOCKETS; ++i)
        if (s_clients[i].used && s_clients[i].fd == fd) return &s_clients[i];
    return NULL;
}
static void abort_session(client_t *c) {
    if (!c || c->failed) return;
    c->failed = true;
    // HTTPD owns the descriptor. Abort now, then let its receive path close it;
    // queued closures can otherwise outlive and target a reused descriptor.
    if (!kp_socket_io_abort(c->fd)) ESP_LOGW("keypad_web", "Cannot abort fd=%d", c->fd);
}
static int send_full(httpd_handle_t http, int fd, const char *data, size_t size, int flags) {
    (void)http; (void)flags;
    client_t *c = client(fd);
    if (!c || c->failed) return HTTPD_SOCK_ERR_FAIL;
    if (!c->handling || !c->sending) {
        kp_socket_io_begin(&c->io, HTTP_SEND_STALL_MS, HTTP_SEND_TOTAL_MS);
        if (c->handling) c->sending = true;
    }
    int sent = kp_socket_io_send(&c->io, fd, data, size);
    if (sent < 0) abort_session(c);
    return sent < 0 ? HTTPD_SOCK_ERR_FAIL : sent;
}
static esp_err_t open_session(httpd_handle_t http, int fd) {
    for (unsigned i = 0; i < HTTP_SOCKETS; ++i) if (!s_clients[i].used) {
        s_clients[i] = (client_t){.used = true, .fd = fd, .used_at = web_now_ms()};
        esp_err_t rc = httpd_sess_set_send_override(http, fd, send_full);
        if (rc != ESP_OK) s_clients[i].used = false;
        return rc;
    }
    return ESP_FAIL;
}
static void close_session(httpd_handle_t http, int fd) {
    (void)http;
    client_t *c = client(fd);
    if (c) c->used = false;
    close(fd);
}
static bool begin_response(int fd) {
    client_t *c = client(fd);
    if (atomic_load(&s_stopping) || !c || c->failed) return false;
    // Start the send deadline on the first response byte, after any request
    // upload or NVS commit. A slow save must not expire its acknowledgement.
    c->handling = true;
    c->sending = false;
    return true;
}
static void end_response(int fd, const char *kind, esp_err_t rc) {
    (void)kind;
    client_t *c = client(fd);
    if (!c) return;
    uint32_t elapsed = c->sending ? web_now_ms() - c->io.started_ms : 0;
    if (rc != ESP_OK || elapsed >= 500U)
        ESP_LOGW("keypad_web", "TX kind=%s fd=%d rc=%d errno=%d blocked_errno=%d bytes=%u waits=%u ms=%u free=%u largest=%u dma=%u dma_largest=%u dma_min=%u",
            kind, fd, (int)rc, c->io.error, c->io.blocked_error, (unsigned)c->io.sent, c->io.waits,
            (unsigned)elapsed, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_DMA));
    c->handling = c->sending = false;
    c->used_at = web_now_ms();
    if (rc != ESP_OK) abort_session(c);
}
static void poll_work(void *arg) {
    (void)arg;
    if (!atomic_load(&s_stopping)) {
        uint32_t now = web_now_ms();
        // All configuration sessions are HTTP, so idle keep-alives and unused
        // browser preconnects can expire even before the pool fills. Releasing
        // them also leaves heap headroom for a later settings upload.
        for (unsigned i = 0; i < HTTP_SOCKETS; ++i) {
            client_t *c = &s_clients[i];
            if (!c->used || c->failed || c->handling || (uint32_t)(now - c->used_at) < HTTP_IDLE_MS) continue;
            // A request may already be waiting behind the current response.
            // Peek without consuming its bytes before classifying it as idle.
            char byte;
            int pending = recv(c->fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
            if (pending > 0 || (pending < 0 && errno == EINTR)) continue;
            // No request waiting is not proof that the last response has been
            // ACKed. A normal shutdown queues FIN after that response; using
            // the failed-send SO_LINGER=0 path here discards it and sends RST.
            c->failed = true;
            shutdown(c->fd, SHUT_RDWR);
        }
        if (s_lan_wifi) {
            kp_wifi_job_t job;
            kp_wifi_setup_get(&job);
            uint64_t time = (uint64_t)(esp_timer_get_time() / 1000);
            kp_web_presence_pause(&s_presence, job.busy, time);
            kp_session_pause(s_generation, job.busy, time);
            // Pages expire independently of the manually enabled function.
            kp_web_presence_poll(&s_presence, time);
            // Only an explicit exit or device transition ends this generation.
            kp_session_view_t session;
            kp_session_get(&session);
            if (session.mode != KP_SESSION_CONFIG || session.generation != s_generation) {
                atomic_store(&s_stopping, true);
            }
        }
    }
    atomic_store(&s_poll_queued, false);
}
void kp_web_poll(uint32_t now) {
    (void)now;
    if (s_http && !atomic_load(&s_stopping) && !atomic_exchange(&s_poll_queued, true))
        if (httpd_queue_work(s_http, poll_work, NULL) != ESP_OK) atomic_store(&s_poll_queued, false);
}
static esp_err_t fail(httpd_req_t *req, const char *status, const char *message) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(req, message);
}
static esp_err_t static_file(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    if (!begin_response(fd)) return ESP_FAIL;
    size_t path_size = strcspn(req->uri, "?");
    if (s_lan_wifi && path_size == 1 && req->uri[0] == '/') {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/config");
        esp_err_t rc = httpd_resp_sendstr(req, "");
        end_response(fd, "redirect", rc);
        return rc;
    }
    if (s_lan_wifi && path_size == 9 && !strncmp(req->uri, "/transfer", 9)) {
        esp_err_t rc = fail(req, "409 Conflict", "请先在设备上退出配置，再开启文件流转");
        end_response(fd, "inactive", rc);
        return rc;
    }
    const char *start = index_start, *end = index_end, *type = "text/html; charset=utf-8";
    if (!strcmp(req->uri, "/app.js")) { start = js_start; end = js_end; type = "text/javascript; charset=utf-8"; }
    else if (!strcmp(req->uri, "/charset.js")) { start = charset_start; end = charset_end; type = "text/javascript; charset=utf-8"; }
    else if (!strcmp(req->uri, "/app.css")) { start = css_start; end = css_end; type = "text/css; charset=utf-8"; }
    if (!strncmp(req->uri, "/config", 7)) httpd_resp_set_hdr(req, "X-Passport-Config", "ready");
    esp_err_t rc = kp_web_asset(req, start, end, type);
    end_response(fd, "asset", rc);
    return rc;
}
esp_err_t kp_web_asset(httpd_req_t *req, const char *start, const char *end, const char *type) {
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'");
    // The length and lifetime are known: send Flash directly with Content-Length.
    // HTTPD/lwIP bound socket writes; httpd_resp_send does not allocate a copy of
    // this asset. Chunked encoding used three sends and an event per 512 bytes,
    // without bounding lwIP's actual pbuf allocations.
    return httpd_resp_send(req, start, end - start - 1);
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
static esp_err_t presence_body(httpd_req_t *req) {
    if (!s_lan_wifi) return fail(req, "409 Conflict", "本机热点无需在线心跳");
    if (!req->content_len || req->content_len > 128) return fail(req, "400 Bad Request", "在线请求无效");
    char body[129];
    body_reader_t reader = {.request = req, .remaining = req->content_len};
    size_t used = 0;
    while (reader.remaining) {
        int count = read_body(&reader, body + used, sizeof(body) - 1 - used);
        if (count <= 0) return fail(req, "400 Bad Request", "在线请求不完整");
        used += (size_t)count;
    }
    body[used] = 0;
    const char *end;
    cJSON *json = cJSON_ParseWithLengthOpts(body, used + 1, &end, true);
    cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "id");
    bool join = cJSON_IsString(action) && !strcmp(action->valuestring, "join");
    bool ping = cJSON_IsString(action) && !strcmp(action->valuestring, "ping");
    bool leave = cJSON_IsString(action) && !strcmp(action->valuestring, "leave");
    cJSON *generation = cJSON_GetObjectItemCaseSensitive(json, "generation");
    cJSON *lease = cJSON_GetObjectItemCaseSensitive(json, "lease");
    bool valid = end == body + used && cJSON_IsObject(json) && cJSON_GetArraySize(json) == (join ? 3 : 2) &&
        (join || ((ping || leave) && cJSON_IsNumber(value) && value->valuedouble >= 1 &&
                  value->valuedouble <= UINT32_MAX && value->valuedouble == (uint32_t)value->valuedouble));
    uint32_t id = valid && !join ? (uint32_t)value->valuedouble : 0;
    if (join) {
        valid = valid && cJSON_IsNumber(generation) && generation->valuedouble == s_generation && cJSON_IsNumber(lease) &&
            lease->valuedouble >= 1 && lease->valuedouble <= UINT32_MAX && lease->valuedouble == (uint32_t)lease->valuedouble;
        if (valid) id = (uint32_t)lease->valuedouble;
    }
    cJSON_Delete(json);
    if (!valid) return fail(req, "400 Bad Request", "在线请求无效");
    uint64_t now = (uint64_t)(esp_timer_get_time() / 1000);
    if (join) {
        if (!kp_session_join(KP_SESSION_CONFIG, s_generation, id, now)) return fail(req, "409 Conflict", "页面连接已失效，请重新连接");
        if (!kp_web_presence_join_id(&s_presence, id, now)) {
            kp_session_leave(KP_SESSION_CONFIG, s_generation, id, now);
            return fail(req, "503 Service Unavailable", "最多四个配置页面，请关闭其他页面后重试");
        }
    } else if (ping && (!kp_session_ping(KP_SESSION_CONFIG, s_generation, id, now) || !kp_web_presence_ping(&s_presence, id, now))) {
        return fail(req, "409 Conflict", "页面在线记录已失效，请重新加入");
    } else if (leave) {
        kp_web_presence_leave(&s_presence, id, now);
        kp_session_leave(KP_SESSION_CONFIG, s_generation, id, now);
    }
    char response[40];
    snprintf(response, sizeof(response), "{\"id\":%lu}", (unsigned long)id);
    esp_err_t rc = httpd_resp_sendstr(req, response);
    if (join && rc != ESP_OK) {
        kp_web_presence_leave(&s_presence, id, now);
        kp_session_leave(KP_SESSION_CONFIG, s_generation, id, now);
    }
    return rc;
}
static esp_err_t api_body(httpd_req_t *req) {
    char host[32], host_port[20], expected[16];
    uint32_t addr = atomic_load(&s_host_ip);
    snprintf(expected, sizeof(expected), "%u.%u.%u.%u", (unsigned)(addr & 255), (unsigned)((addr >> 8) & 255),
             (unsigned)((addr >> 16) & 255), (unsigned)(addr >> 24));
    snprintf(host_port, sizeof(host_port), "%s:80", expected);
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK ||
        (strcmp(host, expected) && strcmp(host, host_port)))
        return fail(req, "403 Forbidden", "请使用设备显示的地址打开配置页");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    if (!strcmp(req->uri, "/api/session") || !strcmp(req->uri, "/api/session/open") || !strcmp(req->uri, "/api/session/resume")) {
        if (!s_lan_wifi) return fail(req, "409 Conflict", "本机热点无需申请局域网页面");
        return kp_session_http(req, expected);
    }
    if (s_lan_wifi && !kp_session_current(KP_SESSION_CONFIG, s_generation)) return fail(req, "403 Forbidden", "配置会话已结束，请重新连接");
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
        kp_system_t system; kp_store_system_get(&system);
        char tail[112]; snprintf(tail, sizeof(tail), ",\"session\":\"%s\",\"connection\":\"%s\",\"system\":%u}", s_session, s_lan_wifi ? "wifi" : "hotspot", (unsigned)system);
        rc = httpd_resp_send_chunk(req, tail, strlen(tail));
        return rc == ESP_OK ? httpd_resp_send_chunk(req, NULL, 0) : rc;
    }
    if (!authorized(req)) return fail(req, "403 Forbidden", "设置会话已失效，请刷新页面");
    if (!strcmp(req->uri, "/api/presence")) return presence_body(req);
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
        if (!settings) {
            ESP_LOGE("keypad_web", "Settings allocation failed free=%u largest=%u needed=%u dma=%u",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                (unsigned)sizeof(*settings),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
            return fail(req, "500 Internal Server Error", "内存不足");
        }
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
    if (rc == ESP_OK) {
        if (s_lan_wifi) kp_session_end(KP_SESSION_CONFIG);
        if (s_request) s_request(forget);
    }
    return rc;
}
static esp_err_t api(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    if (!begin_response(fd)) return ESP_FAIL;
    bool hold = s_lan_wifi && authorized(req) && strcmp(req->uri, "/api/presence");
    if (hold) {
        kp_web_presence_pause(&s_presence, true, (uint64_t)(esp_timer_get_time() / 1000));
        kp_session_pause(s_generation, true, (uint64_t)(esp_timer_get_time() / 1000));
    }
    esp_err_t rc = api_body(req);
    end_response(fd, "api", rc);
    if (hold) {
        kp_wifi_job_t job; kp_wifi_setup_get(&job);
        kp_web_presence_pause(&s_presence, job.busy, (uint64_t)(esp_timer_get_time() / 1000));
        kp_session_pause(s_generation, job.busy, (uint64_t)(esp_timer_get_time() / 1000));
    }
    return rc;
}
esp_err_t kp_web_stop(void) {
    atomic_store(&s_stopping, true);
    if (s_http) {
        esp_err_t rc = httpd_stop(s_http);
        if (rc != ESP_OK) return rc;
        s_http = NULL;
    }
    atomic_store(&s_poll_queued, false);
    s_session[0] = 0;
    if (s_setup_started) {
        esp_err_t stopped = kp_wifi_setup_stop();
        if (stopped != ESP_OK) return stopped;
        s_setup_started = false;
    }
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
void kp_web_update_ip(const char *ip) {
    unsigned a, b, c, d;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a < 256 && b < 256 && c < 256 && d < 256)
        atomic_store(&s_host_ip, a | (b << 8) | (c << 16) | (d << 24));
}
static esp_err_t start(kp_web_request_t callback, const char *ip) {
    if (s_http) return ESP_OK;
    esp_err_t rc = kp_web_stop();
    if (rc != ESP_OK) return rc;
    s_lan_wifi = ip != NULL;
    kp_session_view_t session;
    kp_session_get(&session);
    s_generation = session.generation;
    kp_web_update_ip(ip ? ip : "192.168.4.1");
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ssid, sizeof(s_ssid), "Passport-Keys-%02X%02X", mac[4], mac[5]);
    for (unsigned i = 0; i < 16; ++i) snprintf(s_session + 2 * i, 3, "%02x", (unsigned)(esp_random() & 255));
    s_request = callback;
    if (!ip) {
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
        if (rc == ESP_OK) { s_setup_started = true; rc = kp_wifi_setup_start(false); }
        if (rc == ESP_OK) rc = esp_wifi_start();
        if (rc != ESP_OK) goto fail;
        s_wifi_started = true;
    } else {
        s_setup_started = true;
        rc = kp_wifi_setup_start(true);
        if (rc != ESP_OK) goto fail;
    }
    memset(s_clients, 0, sizeof(s_clients));
    atomic_store(&s_stopping, false);
    kp_web_presence_init(&s_presence);
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.max_open_sockets = HTTP_SOCKETS; http.max_uri_handlers = 24;
    http.backlog_conn = 1;
    // Configuration has no persistent WebSocket peers to protect. Reclaim
    // keep-alives when full rather than retaining the transfer service's six
    // connections and their TCP queues in DMA-capable RAM.
    http.lru_purge_enable = true; http.stack_size = 6144;
    http.recv_wait_timeout = 2; http.send_wait_timeout = 2;
    http.open_fn = open_session; http.close_fn = close_session;
    rc = httpd_start(&s_http, &http);
    if (rc != ESP_OK) goto fail;
    ESP_LOGI("keypad_web", "HTTP ready sockets=%u dma=%u dma_largest=%u rtc=%u",
        (unsigned)http.max_open_sockets,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_RTCRAM));
    const httpd_uri_t routes[] = {
        {.uri="/api/session",.method=HTTP_GET,.handler=api},
        {.uri="/api/session/open",.method=HTTP_POST,.handler=api},
        {.uri="/api/session/resume",.method=HTTP_POST,.handler=api},
        {.uri="/", .method=HTTP_GET, .handler=static_file},
        {.uri="/config", .method=HTTP_GET, .handler=static_file},
        {.uri="/transfer", .method=HTTP_GET, .handler=static_file},
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
        {.uri="/api/presence", .method=HTTP_POST, .handler=api},
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
esp_err_t kp_web_start(kp_web_request_t callback) { return start(callback, NULL); }
esp_err_t kp_web_start_wifi(const char *ip, kp_web_request_t callback) {
    if (!ip || !*ip || strlen(ip) >= 16) return ESP_ERR_INVALID_ARG;
    return start(callback, ip);
}
