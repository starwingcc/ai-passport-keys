#include "keypad_transfer_web.h"
#include "keypad_session_http.h"
#include "cJSON.h"
#include "keypad_transfer_signal.h"
#include "keypad_socket_io.h"
#include "keypad_transfer_diag.h"
#include "keypad_web_asset.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
static httpd_handle_t s_http;
// Catalogs belong to the running transfer service, not to configuration mode.
static kp_transfer_signal_t *s_signal;
static char s_ip[16];
static atomic_bool s_poll_queued, s_stopping, s_active, s_draining;
static SemaphoreHandle_t s_control_done;
static atomic_uint s_control_state;
static kp_transfer_presence_t s_presence;
static portMUX_TYPE s_presence_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_generation, s_session_generation;
static kp_transfer_signal_t *s_prepared, *s_retired;
static unsigned s_release_phase;
static bool s_release_grace;
static bool s_ready;
#define HTTP_SOCKETS 6
// Allow an initial TCP retransmission before retiring a silent keep-alive.
#define HTTP_IDLE_MS 5000U
#define TRANSFER_SEND_MS 2000U
// Same bounded response policy as the configuration webpage. Unlike a small
// signaling frame, a Flash asset can span TCP retransmission/modem wake cycles.
#define ASSET_SEND_STALL_MS 5000U
#define ASSET_SEND_TOTAL_MS 30000U
typedef struct {
    int fd;
    uint32_t opened;
    bool used, sending, failed, aborted;
    uint32_t generation, lease;
    kp_socket_io_t io;
} client_t;
static client_t s_clients[HTTP_SOCKETS];
extern const char transfer_html_start[] __asm__("_binary_transfer_html_start"), transfer_html_end[] __asm__("_binary_transfer_html_end");
extern const char transfer_js_start[] __asm__("_binary_transfer_js_start"), transfer_js_end[] __asm__("_binary_transfer_js_end");
extern const char transfer_css_start[] __asm__("_binary_transfer_css_start"), transfer_css_end[] __asm__("_binary_transfer_css_end");
extern const char transfer_protocol_start[] __asm__("_binary_transfer_protocol_js_start"), transfer_protocol_end[] __asm__("_binary_transfer_protocol_js_end");
static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time()/1000); }
void kp_transfer_web_presence(kp_transfer_presence_t *out) {
    portENTER_CRITICAL(&s_presence_lock);
    *out = s_presence;
    portEXIT_CRITICAL(&s_presence_lock);
}
// HTTP thread publishes the fields consumed by the network worker.
static void presence_publish(void) {
    kp_transfer_presence_t next = {
        .online = s_signal ? kp_transfer_signal_online(s_signal, now_ms()) : 0,
        .allocated = s_signal != NULL,
        .generation = s_generation,
    };
    portENTER_CRITICAL(&s_presence_lock);
    s_presence = next;
    portEXIT_CRITICAL(&s_presence_lock);
}
static client_t *client(int fd) {
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used&&s_clients[i].fd==fd)return &s_clients[i];
    return NULL;
}
static bool abort_session(int fd) {
    client_t *c=client(fd);
    if(c&&c->aborted)return true;
    if(c)c->failed=true;
    bool ok=kp_socket_io_abort(fd);
    if(c)c->aborted=ok;
    if(!ok)ESP_LOGW("transfer","Cannot set abortive close fd=%d",fd);
    return ok;
}
static void begin_send(int fd) {
    client_t *c=client(fd);
    if(c){kp_socket_io_begin(&c->io,TRANSFER_SEND_MS,TRANSFER_SEND_MS);c->sending=true;}
}
static void begin_asset_send(int fd) {
    client_t *c=client(fd);
    if(c){kp_socket_io_begin(&c->io,ASSET_SEND_STALL_MS,ASSET_SEND_TOTAL_MS);c->sending=true;}
}
static void end_send(int fd, const char *kind, esp_err_t result) {
    client_t *c=client(fd);
    if(!c)return;
    uint32_t elapsed=(uint32_t)now_ms()-c->io.started_ms;
    if(result!=ESP_OK||elapsed>=500U) {
        ESP_LOGW("transfer","TX kind=%s fd=%d rc=%d errno=%d blocked_errno=%d bytes=%u waits=%u ms=%u free=%u largest=%u",
            kind,fd,(int)result,c->io.error,c->io.blocked_error,(unsigned)c->io.sent,c->io.waits,(unsigned)elapsed,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        kp_transfer_diag_request();
    }
    c->sending=false;
    c->opened=(uint32_t)now_ms();
    if(result!=ESP_OK)abort_session(fd);
}
static int send_full(httpd_handle_t http, int fd, const char *data, size_t size, int flags) {
    (void)http;(void)flags;
    client_t *c=client(fd);
    if(!c||c->failed)return HTTPD_SOCK_ERR_FAIL;
    if(!c->sending)kp_socket_io_begin(&c->io,TRANSFER_SEND_MS,TRANSFER_SEND_MS);
    int sent=kp_socket_io_send(&c->io,fd,data,size);
    if(sent<0)abort_session(fd);
    return sent<0?HTTPD_SOCK_ERR_FAIL:sent;
}
static bool send_message(void *user, int fd, const char *data, size_t size) {
    (void)user;
    begin_send(fd);
    httpd_ws_frame_t frame={.type=HTTPD_WS_TYPE_TEXT,.payload=(uint8_t *)data,.len=size};
    esp_err_t rc=httpd_ws_send_frame_async(s_http,fd,&frame);
    end_send(fd,"signal",rc);
    return rc==ESP_OK;
}
static esp_err_t open_session(httpd_handle_t http, int fd) {
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(!s_clients[i].used) {
        s_clients[i]=(client_t){.used=true,.fd=fd,.opened=(uint32_t)now_ms()};
        return httpd_sess_set_send_override(http,fd,send_full);
    }
    return ESP_FAIL;
}
static void close_session(httpd_handle_t http, int fd) {
    (void)http;
    client_t *c=client(fd);
    uint32_t generation=c?c->generation:0;
    uint32_t lease = c ? c->lease : 0;
    if (lease) kp_session_leave(KP_SESSION_TRANSFER, s_session_generation, lease, now_ms());
    presence_publish();
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used&&s_clients[i].fd==fd)s_clients[i].used=false;
    close(fd);
    if(!s_signal||generation!=s_generation)return;
    for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)if(s_signal->peers[i].id&&s_signal->peers[i].fd==fd)
        ESP_LOGI("transfer","Browser session closed id=%u stopping=%d",(unsigned)s_signal->peers[i].id,atomic_load(&s_stopping));
    if(!atomic_load(&s_active)) {
        kp_transfer_signal_discard(s_signal,fd);
    } else kp_transfer_signal_close(s_signal,fd);
    presence_publish();
}
static esp_err_t fail(httpd_req_t *r, const char *status, const char *text) {httpd_resp_set_status(r,status);return httpd_resp_sendstr(r,text);}
static bool valid_host(httpd_req_t *r, bool origin) {
    char host[32],expected[40];
    if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK)return false;
    snprintf(expected,sizeof(expected),"%s:80",s_ip);
    if(strcmp(host,s_ip)&&strcmp(host,expected))return false;
    if(!origin)return true;
    char value[48];
    if(httpd_req_get_hdr_value_str(r,"Origin",value,sizeof(value))!=ESP_OK)return false;
    snprintf(expected,sizeof(expected),"http://%s",host);
    return !strcmp(value,expected);
}
static esp_err_t pre_handshake(httpd_req_t *r) {
    if(atomic_load(&s_stopping)||!valid_host(r,true)){fail(r,"403 Forbidden","Use the address on Passport");return ESP_FAIL;}
    if(!atomic_load(&s_active)||!s_signal||!kp_session_current(KP_SESSION_TRANSFER,s_session_generation)){fail(r,"503 Service Unavailable","Transfer is starting; retry shortly");return ESP_FAIL;}
    unsigned reserved=0;
    for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)reserved+=s_signal->peers[i].id!=0;
    if(reserved>=KP_TRANSFER_PEERS){fail(r,"503 Service Unavailable","Four browser sessions maximum");return ESP_FAIL;}
    return ESP_OK;
}
static esp_err_t websocket(httpd_req_t *r) {
    if(atomic_load(&s_stopping)||!atomic_load(&s_active)||!s_signal)return ESP_FAIL;
    int fd=httpd_req_to_sockfd(r);
    client_t *c=client(fd);
    if(!c)return ESP_FAIL;
    if(r->method==HTTP_GET) {
        c->generation=s_generation;
        return kp_transfer_signal_open(s_signal,fd,now_ms())?ESP_OK:ESP_FAIL;
    }
    if(c->generation!=s_generation||!kp_session_current(KP_SESSION_TRANSFER,s_session_generation))return ESP_FAIL;
    presence_publish();
    httpd_ws_frame_t frame={0};esp_err_t rc=httpd_ws_recv_frame(r,&frame,0);
    if(rc!=ESP_OK)return rc;
    // Browser signaling messages are single text frames. Reject fragmentation,
    // binary content and oversized frames before allocating their payload.
    if(frame.type!=HTTPD_WS_TYPE_TEXT||!frame.final||frame.fragmented||!frame.len||frame.len>KP_TRANSFER_MESSAGE){
        ESP_LOGW("transfer","Rejected frame fd=%d type=%d bytes=%u final=%d",fd,(int)frame.type,(unsigned)frame.len,frame.final);return ESP_FAIL;}
    char *buffer=malloc(frame.len+1);if(!buffer)return ESP_ERR_NO_MEM;
    frame.payload=(uint8_t *)buffer;rc=httpd_ws_recv_frame(r,&frame,frame.len);buffer[frame.len]=0;
    bool admitted = rc == ESP_OK;
    if (admitted && !c->lease) {
        cJSON *msg = cJSON_Parse(buffer);
        cJSON *type = cJSON_GetObjectItemCaseSensitive(msg, "type");
        cJSON *gen = cJSON_GetObjectItemCaseSensitive(msg, "generation");
        cJSON *lease = cJSON_GetObjectItemCaseSensitive(msg, "lease");
        admitted = cJSON_IsString(type) && !strcmp(type->valuestring, "hello") && cJSON_IsNumber(gen) && cJSON_IsNumber(lease) &&
            gen->valuedouble == s_session_generation && lease->valuedouble >= 1 && lease->valuedouble <= UINT32_MAX &&
            lease->valuedouble == (uint32_t)lease->valuedouble &&
            kp_session_join(KP_SESSION_TRANSFER, s_session_generation, (uint32_t)lease->valuedouble, now_ms());
        if (admitted) c->lease = (uint32_t)lease->valuedouble;
        cJSON_Delete(msg);
    }
    bool ok = admitted && kp_transfer_signal_message(s_signal, fd, buffer, frame.len, now_ms());
    free(buffer);
    if (ok) ok = kp_session_ping(KP_SESSION_TRANSFER, s_session_generation, c->lease, now_ms());
    if(!ok)ESP_LOGW("transfer","Signaling receive rejected fd=%d rc=%d bytes=%u free=%u",fd,(int)rc,(unsigned)frame.len,(unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    presence_publish();
    // Invalid messages terminate the offending session, so it cannot flood an
    // unbounded error queue. Content is never printed in logs.
    return ok?ESP_OK:ESP_FAIL;
}
static esp_err_t asset_body(httpd_req_t *r) {
    if(atomic_load(&s_stopping)||!valid_host(r,false))return fail(r,"403 Forbidden","Use the address on Passport");
    if(atomic_load(&s_draining))return fail(r,"503 Service Unavailable","Retry shortly");
    size_t path_size=strcspn(r->uri,"?");
    bool root=path_size==1&&!strncmp(r->uri,"/",1);
    bool page=path_size==9&&!strncmp(r->uri,"/transfer",9);
    if(path_size==7&&!strncmp(r->uri,"/config",7)) return fail(r,"409 Conflict","Enable system configuration on Passport first");
    if(root){httpd_resp_set_status(r,"302 Found");httpd_resp_set_hdr(r,"Location","/transfer");return httpd_resp_sendstr(r,"");}
    if(path_size==12&&!strncmp(r->uri,"/transfer.js",12))return kp_web_asset(r,transfer_js_start,transfer_js_end,"text/javascript; charset=utf-8");
    if(path_size==21&&!strncmp(r->uri,"/transfer-protocol.js",21))return kp_web_asset(r,transfer_protocol_start,transfer_protocol_end,"text/javascript; charset=utf-8");
    if(path_size==13&&!strncmp(r->uri,"/transfer.css",13))return kp_web_asset(r,transfer_css_start,transfer_css_end,"text/css; charset=utf-8");
    if(!page)return fail(r,"404 Not Found","Not found");
    httpd_resp_set_hdr(r,"X-Passport-Transfer","ready");
    return kp_web_asset(r,transfer_html_start,transfer_html_end,"text/html; charset=utf-8");
}
static esp_err_t asset(httpd_req_t *r) {
    // Reuse browser HTTP connections. Actively closing every asset leaves TCP
    // TIME_WAIT state on Passport and exhausts heap across repeated refreshes.
    int fd=httpd_req_to_sockfd(r);
    begin_asset_send(fd);
    esp_err_t rc=asset_body(r);
    end_send(fd,"asset",rc);
    return rc;
}
static esp_err_t session_api(httpd_req_t *r) {
    begin_asset_send(httpd_req_to_sockfd(r));
    esp_err_t rc=kp_session_http(r,s_ip);
    end_send(httpd_req_to_sockfd(r),"session",rc);
    return rc;
}
static void poll_work(void *arg) {
    uint32_t generation=(uint32_t)(uintptr_t)arg;
    presence_publish();
    int fd;
    while(s_signal&&generation==s_generation&&(fd=kp_transfer_signal_expired(s_signal,now_ms()))>=0) {
        ESP_LOGW("transfer","Browser heartbeat expired fd=%d",fd);
        abort_session(fd);kp_transfer_signal_close(s_signal,fd);
    }
    // SO_RCVTIMEO cannot expire a preconnect/keep-alive waiting in select().
    // Reclaim idle HTTP even below capacity: otherwise unacknowledged replies
    // can retain packet buffers indefinitely and starve Wi-Fi. This runs on
    // HTTPD after the response finishes; live WebSockets are never evicted.
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used){
        client_t *c=&s_clients[i];
        uint32_t elapsed=(uint32_t)now_ms()-c->opened;
        if(!c->sending&&elapsed>=HTTP_IDLE_MS&&httpd_ws_get_fd_info(s_http,c->fd)==HTTPD_WS_CLIENT_HTTP)
            abort_session(c->fd);
    }
    presence_publish();atomic_store(&s_poll_queued,false);
}
void kp_transfer_web_poll(uint64_t now) {
    (void)now;
    if(s_http&&!atomic_load(&s_stopping)&&!atomic_exchange(&s_poll_queued,true)) {
        kp_transfer_presence_t p;kp_transfer_web_presence(&p);
        if(httpd_queue_work(s_http,poll_work,(void *)(uintptr_t)p.generation)!=ESP_OK)atomic_store(&s_poll_queued,false);
    }
}
// Commands contain no borrowed caller pointers. A timed-out command stays owned
// until its completion is consumed; its semaphore and prepared/retired catalogs
// survive every failed stop. Only the network worker submits these commands.
typedef enum { CONTROL_ACTIVATE, CONTROL_GATE, CONTROL_RELEASE } control_kind_t;
static struct {
    control_kind_t kind;
    esp_err_t result;
} s_control;
static bool abort_browsers(void) {
    bool ok = true;
    for (unsigned i = 0; i < HTTP_SOCKETS; ++i)
        if (s_clients[i].used && s_clients[i].generation && !abort_session(s_clients[i].fd)) ok = false;
    return ok;
}
static void control_work(void *arg) {
    (void)arg;
    esp_err_t rc = ESP_OK;
    presence_publish();
    if (s_control.kind == CONTROL_ACTIVATE) {
        if (atomic_load(&s_stopping) || atomic_load(&s_draining)) rc = ESP_ERR_INVALID_STATE;
        else {
            if (!s_signal) {
                s_signal = s_prepared; s_prepared = NULL;
                if (!s_signal) rc = ESP_ERR_NO_MEM;
                else {
                    ++s_generation; if (!s_generation) ++s_generation;
                    portENTER_CRITICAL(&s_presence_lock);
                    s_presence = (kp_transfer_presence_t){.allocated=true, .generation=s_generation};
                    portEXIT_CRITICAL(&s_presence_lock);
                }
            }
            if (rc == ESP_OK) {
                kp_session_view_t session;
                kp_session_get(&session);
                s_session_generation = session.generation;
                atomic_store(&s_active, true);
            }
        }
    } else if (s_control.kind == CONTROL_GATE) {
        atomic_store(&s_active, false); atomic_store(&s_draining, true);
        if (s_signal) kp_transfer_signal_end(s_signal);
    } else if (s_control.kind == CONTROL_RELEASE) {
        if (!abort_browsers()) rc = ESP_FAIL;
        else {
            s_retired = s_signal; s_signal = NULL;
            atomic_store(&s_active, false); atomic_store(&s_draining, false);
        }
    }
    presence_publish();
    s_control.result = rc;
    atomic_store(&s_control_state, 2);
    xSemaphoreGive(s_control_done);
}
static void control_cleanup(control_kind_t kind, esp_err_t rc) {
    if (kind == CONTROL_ACTIVATE && s_prepared) { free(s_prepared); s_prepared = NULL; }
    if (kind == CONTROL_GATE) {
        s_release_phase = rc == ESP_OK ? 2 : 0;
        s_release_grace = rc == ESP_OK;
    }
    if (kind == CONTROL_RELEASE && rc == ESP_OK) {
        kp_transfer_signal_clear(s_retired);
        free(s_retired); s_retired = NULL; s_release_phase = 0;
    }
}
static esp_err_t control_finish(void) {
    if (atomic_load(&s_control_state) == 1) {
        if (xSemaphoreTake(s_control_done, pdMS_TO_TICKS(6000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    } else (void)xSemaphoreTake(s_control_done, 0);
    esp_err_t rc = s_control.result;
    control_cleanup(s_control.kind, rc);
    atomic_store(&s_control_state, 0);
    return rc;
}
static esp_err_t control(control_kind_t kind) {
    if (atomic_load(&s_control_state)) {
        control_kind_t previous = s_control.kind;
        esp_err_t rc = control_finish();
        if (rc != ESP_OK || previous == kind) return rc;
    }
    if (kind == CONTROL_ACTIVATE) {
        kp_transfer_presence_t p; kp_transfer_web_presence(&p);
        if (!p.allocated) {
            s_prepared = malloc(sizeof(*s_prepared));
            if (!s_prepared) return ESP_ERR_NO_MEM;
            kp_transfer_signal_init(s_prepared, send_message, NULL);
        }
    }
    s_control.kind = kind;
    atomic_store(&s_control_state, 1);
    if (httpd_queue_work(s_http, control_work, NULL) != ESP_OK) {
        atomic_store(&s_control_state, 0); control_cleanup(kind, ESP_FAIL); return ESP_FAIL;
    }
    return control_finish();
}
static esp_err_t release_resources(void) {
    if (s_release_phase != 2) {
        s_release_phase = 1;
        esp_err_t rc = control(CONTROL_GATE);
        if (rc != ESP_OK) return rc;
    }
    if (s_release_grace) { s_release_grace = false; vTaskDelay(pdMS_TO_TICKS(300)); }
    return control(CONTROL_RELEASE);
}
esp_err_t kp_transfer_web_stop(void) {
    if (!s_http) return ESP_OK;
    atomic_store(&s_stopping, true); s_ready = false;
    esp_err_t rc = release_resources();
    if (rc != ESP_OK) return rc;
    rc = httpd_stop(s_http);
    if (rc != ESP_OK) return rc;
    // HTTPD joined its task and closed all sockets. Nothing queued can refer
    // to this instance or its semaphore after ownership is released.
    s_http = NULL;
    atomic_store(&s_poll_queued, false);
    vSemaphoreDelete(s_control_done); s_control_done = NULL;
    return ESP_OK;
}
static esp_err_t http_start(const char *ip) {
    if (!ip || !*ip || strlen(ip) >= sizeof(s_ip)) return ESP_ERR_INVALID_ARG;
    if (s_http) {
        if (!s_ready || atomic_load(&s_stopping) || strcmp(ip, s_ip)) return ESP_ERR_INVALID_STATE;
        return ESP_OK;
    }
    s_ready = false; s_release_phase = 0; s_release_grace = false;
    atomic_store(&s_control_state, 0);
    snprintf(s_ip, sizeof(s_ip), "%s", ip);
    atomic_store(&s_stopping, false); atomic_store(&s_poll_queued, false);
    atomic_store(&s_active, false); atomic_store(&s_draining, false);
    portENTER_CRITICAL(&s_presence_lock); s_presence = (kp_transfer_presence_t){0}; portEXIT_CRITICAL(&s_presence_lock);
    memset(s_clients, 0, sizeof(s_clients));
    s_control_done = xSemaphoreCreateBinary();
    if (!s_control_done) return ESP_ERR_NO_MEM;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); config.max_open_sockets = HTTP_SOCKETS; config.max_uri_handlers = 10;
    config.backlog_conn = 1;
    config.stack_size = 6144; config.lru_purge_enable = false; config.recv_wait_timeout = 2; config.send_wait_timeout = 2;
    config.open_fn = open_session; config.close_fn = close_session;
    esp_err_t rc = httpd_start(&s_http, &config);
    if (rc != ESP_OK) { vSemaphoreDelete(s_control_done); s_control_done = NULL; return rc; }
    const httpd_uri_t routes[] = {
        {.uri="/api/session",.method=HTTP_GET,.handler=session_api},
        {.uri="/api/session/open",.method=HTTP_POST,.handler=session_api},
        {.uri="/api/session/resume",.method=HTTP_POST,.handler=session_api},
        {.uri="/", .method=HTTP_GET, .handler=asset}, {.uri="/transfer", .method=HTTP_GET, .handler=asset},
        {.uri="/transfer.js", .method=HTTP_GET, .handler=asset}, {.uri="/transfer.css", .method=HTTP_GET, .handler=asset},
        {.uri="/transfer-protocol.js", .method=HTTP_GET, .handler=asset},
        {.uri="/config", .method=HTTP_GET, .handler=asset},
        {.uri="/transfer/ws", .method=HTTP_GET, .handler=websocket, .is_websocket=true, .ws_pre_handshake_cb=pre_handshake}
    };
    for (unsigned i = 0; i < sizeof(routes)/sizeof(routes[0]); ++i)
        if ((rc = httpd_register_uri_handler(s_http, &routes[i])) != ESP_OK) { kp_transfer_web_stop(); return rc; }
    s_ready = true;
    ESP_LOGI("transfer", "Manual transfer HTTP ready");
    return ESP_OK;
}
esp_err_t kp_transfer_web_start(const char *ip) {
    if (!ip || !*ip || strlen(ip) >= sizeof(s_ip)) return ESP_ERR_INVALID_ARG;
    kp_session_view_t session;
    kp_session_get(&session);
    if (session.mode != KP_SESSION_TRANSFER) return ESP_ERR_INVALID_STATE;
    // A failed HTTP initialization, stop, or changed generation must finish
    // teardown before another instance can acquire its sockets and buffers.
    if (s_http && (!s_ready || atomic_load(&s_stopping) || s_release_phase ||
        strcmp(ip, s_ip) || (atomic_load(&s_active) && session.generation != s_session_generation))) {
        esp_err_t rc = kp_transfer_web_stop();
        if (rc != ESP_OK) return rc;
    }
    esp_err_t rc = http_start(ip);
    if (rc != ESP_OK) return rc;
    if (kp_transfer_web_ready() && !atomic_load(&s_control_state)) return ESP_OK;
    return control(CONTROL_ACTIVATE);
}
bool kp_transfer_web_running(void) { return s_http != NULL; }
bool kp_transfer_web_ready(void) {
    return s_http && s_ready && !atomic_load(&s_stopping) && atomic_load(&s_active) &&
        kp_session_current(KP_SESSION_TRANSFER, s_session_generation);
}
