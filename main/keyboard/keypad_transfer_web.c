#include "keypad_transfer_web.h"
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
static atomic_bool s_poll_queued, s_stopping, s_active, s_draining, s_link_up;
static SemaphoreHandle_t s_control_done;
static atomic_uint s_control_state;
static kp_transfer_wake_t s_wake, s_config_wake;
static kp_transfer_presence_t s_presence;
static portMUX_TYPE s_presence_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_generation;
static kp_transfer_signal_t *s_prepared, *s_retired;
static unsigned s_release_phase;
static bool s_release_grace;
static bool s_ready;
#define HTTP_SOCKETS 6
#define HTTP_IDLE_MS 2000U
#define TRANSFER_SEND_MS 2000U
// Same bounded response policy as the configuration webpage. Unlike a small
// signaling frame, a Flash asset can span TCP retransmission/modem wake cycles.
#define ASSET_SEND_STALL_MS 5000U
#define ASSET_SEND_TOTAL_MS 30000U
typedef struct {
    int fd;
    uint32_t opened;
    bool used, sending, failed, aborted;
    uint32_t generation;
    kp_socket_io_t io;
} client_t;
static client_t s_clients[HTTP_SOCKETS];
extern const char transfer_html_start[] __asm__("_binary_transfer_html_start"), transfer_html_end[] __asm__("_binary_transfer_html_end");
extern const char transfer_js_start[] __asm__("_binary_transfer_js_start"), transfer_js_end[] __asm__("_binary_transfer_js_end");
extern const char transfer_css_start[] __asm__("_binary_transfer_css_start"), transfer_css_end[] __asm__("_binary_transfer_css_end");
extern const char transfer_protocol_start[] __asm__("_binary_transfer_protocol_js_start"), transfer_protocol_end[] __asm__("_binary_transfer_protocol_js_end");
extern const char config_wait_start[] __asm__("_binary_config_wait_html_start"), config_wait_end[] __asm__("_binary_config_wait_html_end");
static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time()/1000); }
void kp_transfer_web_presence(kp_transfer_presence_t *out) {
    portENTER_CRITICAL(&s_presence_lock);
    *out = s_presence;
    portEXIT_CRITICAL(&s_presence_lock);
}
// HTTP thread only. Preserve the actual last expiration time, rather than
// adding the polling delay to the 120-second heartbeat interval.
static void presence_publish_at(bool explicit_close) {
    uint64_t now = now_ms();
    kp_transfer_presence_t next;
    kp_transfer_web_presence(&next);
    unsigned online = s_signal ? kp_transfer_signal_online(s_signal, now) : 0;
    if (next.online && !online) {
        uint64_t lag = UINT64_MAX;
        if (!explicit_close && s_signal) for (unsigned i = 0; i < KP_TRANSFER_PEERS; ++i) {
            kp_transfer_peer_t *p = &s_signal->peers[i];
            if (p->id && p->joined && now - p->last_ms >= KP_TRANSFER_IDLE_MS) {
                uint64_t elapsed = now - p->last_ms - KP_TRANSFER_IDLE_MS;
                if (elapsed < lag) lag = elapsed;
            }
        }
        next.zero_since_ms = lag == UINT64_MAX ? now : now - lag;
    }
    next.online = online;
    next.allocated = s_signal != NULL;
    next.generation = s_generation;
    portENTER_CRITICAL(&s_presence_lock);
    s_presence = next;
    portEXIT_CRITICAL(&s_presence_lock);
}
static void presence_publish(void) { presence_publish_at(false); }
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
    presence_publish();
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used&&s_clients[i].fd==fd)s_clients[i].used=false;
    close(fd);
    if(!s_signal||generation!=s_generation)return;
    for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)if(s_signal->peers[i].id&&s_signal->peers[i].fd==fd)
        ESP_LOGI("transfer","Browser session closed id=%u stopping=%d",(unsigned)s_signal->peers[i].id,atomic_load(&s_stopping));
    if(!atomic_load(&s_active)) {
        for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)if(s_signal->peers[i].id&&s_signal->peers[i].fd==fd)memset(&s_signal->peers[i],0,sizeof(s_signal->peers[i]));
    } else kp_transfer_signal_close(s_signal,fd);
    presence_publish_at(true);
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
    if(!atomic_load(&s_active)||!atomic_load(&s_link_up)||!s_signal){fail(r,"503 Service Unavailable","Transfer is starting; retry shortly");return ESP_FAIL;}
    unsigned reserved=0;
    for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)reserved+=s_signal->peers[i].id!=0;
    if(reserved>=KP_TRANSFER_PEERS){fail(r,"503 Service Unavailable","Four browser sessions maximum");return ESP_FAIL;}
    return ESP_OK;
}
static esp_err_t websocket(httpd_req_t *r) {
    if(atomic_load(&s_stopping)||!atomic_load(&s_active)||!atomic_load(&s_link_up)||!s_signal)return ESP_FAIL;
    int fd=httpd_req_to_sockfd(r);
    client_t *c=client(fd);
    if(!c)return ESP_FAIL;
    if(r->method==HTTP_GET) {
        c->generation=s_generation;
        return kp_transfer_signal_open(s_signal,fd,now_ms())?ESP_OK:ESP_FAIL;
    }
    if(c->generation!=s_generation)return ESP_FAIL;
    presence_publish();
    httpd_ws_frame_t frame={0};esp_err_t rc=httpd_ws_recv_frame(r,&frame,0);
    if(rc!=ESP_OK)return rc;
    // Browser signaling messages are single text frames. Reject fragmentation,
    // binary content and oversized frames before allocating their payload.
    if(frame.type!=HTTPD_WS_TYPE_TEXT||!frame.final||frame.fragmented||!frame.len||frame.len>KP_TRANSFER_MESSAGE){
        ESP_LOGW("transfer","Rejected frame fd=%d type=%d bytes=%u final=%d",fd,(int)frame.type,(unsigned)frame.len,frame.final);return ESP_FAIL;}
    char *buffer=malloc(frame.len+1);if(!buffer)return ESP_ERR_NO_MEM;
    frame.payload=(uint8_t *)buffer;rc=httpd_ws_recv_frame(r,&frame,frame.len);buffer[frame.len]=0;
    bool ok=rc==ESP_OK&&kp_transfer_signal_message(s_signal,fd,buffer,frame.len,now_ms());free(buffer);
    if(!ok)ESP_LOGW("transfer","Signaling receive rejected fd=%d rc=%d bytes=%u free=%u",fd,(int)rc,(unsigned)frame.len,(unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    presence_publish();
    // Invalid messages terminate the offending session, so it cannot flood an
    // unbounded error queue. Content is never printed in logs.
    return ok?ESP_OK:ESP_FAIL;
}
static esp_err_t asset_body(httpd_req_t *r) {
    if(atomic_load(&s_stopping)||!valid_host(r,false))return fail(r,"403 Forbidden","Use the address on Passport");
    if(!atomic_load(&s_link_up)||atomic_load(&s_draining))return fail(r,"503 Service Unavailable","Retry shortly");
    size_t path_size=strcspn(r->uri,"?");
    bool root=path_size==1&&!strncmp(r->uri,"/",1);
    bool page=path_size==9&&!strncmp(r->uri,"/transfer",9);
    if(path_size==7&&!strncmp(r->uri,"/config",7)) {
        // One response contains its retry script, so HTTP/radio handoff cannot
        // strand a browser waiting for another bootstrap asset. Allow only
        // this script hash; the configuration editor retains its existing CSP.
        httpd_resp_set_type(r,"text/html; charset=utf-8");
        httpd_resp_set_hdr(r,"Cache-Control","no-store");
        httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
        httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'none'; script-src 'sha256-44hK36WvyljLWtTAGERsNI7eHANPcgW4bVjS1YyEhEc='; connect-src 'self'; frame-ancestors 'none'");
        esp_err_t rc=httpd_resp_send(r,config_wait_start,config_wait_end-config_wait_start-1);
        if(rc==ESP_OK&&s_config_wake)s_config_wake(now_ms());
        return rc;
    }
    if((root||page)&&r->method==HTTP_GET&&!atomic_load(&s_active)&&s_wake)s_wake(now_ms());
    if(root){httpd_resp_set_status(r,"302 Found");httpd_resp_set_hdr(r,"Location","/transfer");return httpd_resp_sendstr(r,"");}
    if(path_size==12&&!strncmp(r->uri,"/transfer.js",12))return kp_web_asset(r,transfer_js_start,transfer_js_end,"text/javascript; charset=utf-8");
    if(path_size==21&&!strncmp(r->uri,"/transfer-protocol.js",21))return kp_web_asset(r,transfer_protocol_start,transfer_protocol_end,"text/javascript; charset=utf-8");
    if(path_size==13&&!strncmp(r->uri,"/transfer.css",13))return kp_web_asset(r,transfer_css_start,transfer_css_end,"text/css; charset=utf-8");
    if(!page)return fail(r,"404 Not Found","Not found");
    return kp_web_asset(r,transfer_html_start,transfer_html_end,"text/html; charset=utf-8");
}
static esp_err_t asset(httpd_req_t *r) {
    // Reuse browser HTTP connections. Actively closing every asset leaves TCP
    // TIME_WAIT state on Passport and exhausts heap across repeated refreshes.
    int fd=httpd_req_to_sockfd(r);
    begin_asset_send(fd);
    esp_err_t rc=asset_body(r);
    end_send(fd,"asset",rc);
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used&&s_clients[i].fd==fd)s_clients[i].opened=(uint32_t)now_ms();
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
    // SO_RCVTIMEO cannot expire a preconnect waiting in select(). When all
    // slots are occupied, reclaim the oldest idle HTTP socket only. Global
    // LRU eviction would also evict live WebSockets during a refresh burst.
    unsigned occupied=0;int idle=-1;uint32_t age=HTTP_IDLE_MS-1;
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used){
        ++occupied;uint32_t elapsed=(uint32_t)now_ms()-s_clients[i].opened;
        if(elapsed>age&&httpd_ws_get_fd_info(s_http,s_clients[i].fd)==HTTPD_WS_CLIENT_HTTP){idle=(int)i;age=elapsed;}
    }
    if(occupied==HTTP_SOCKETS&&idle>=0)abort_session(s_clients[idle].fd);
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
typedef enum { CONTROL_ACTIVATE, CONTROL_GATE, CONTROL_RELEASE, CONTROL_DISCONNECT } control_kind_t;
static struct {
    control_kind_t kind;
    bool force;
    uint64_t zero_since_ms;
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
        if (atomic_load(&s_stopping) || atomic_load(&s_draining) || !atomic_load(&s_link_up)) rc = ESP_ERR_INVALID_STATE;
        else {
            if (!s_signal) {
                s_signal = s_prepared; s_prepared = NULL;
                if (!s_signal) rc = ESP_ERR_NO_MEM;
                else {
                    ++s_generation; if (!s_generation) ++s_generation;
                    portENTER_CRITICAL(&s_presence_lock);
                    s_presence = (kp_transfer_presence_t){.allocated=true, .generation=s_generation, .zero_since_ms=s_control.zero_since_ms};
                    portEXIT_CRITICAL(&s_presence_lock);
                }
            }
            if (rc == ESP_OK) atomic_store(&s_active, true);
        }
    } else if (s_control.kind == CONTROL_GATE) {
        kp_transfer_presence_t p; kp_transfer_web_presence(&p);
        if (!s_control.force && p.allocated && (p.online || now_ms() - p.zero_since_ms < KP_TRANSFER_QUIET_MS)) rc = ESP_ERR_INVALID_STATE;
        else {
            atomic_store(&s_active, false); atomic_store(&s_draining, true);
            if (s_control.force && s_signal) kp_transfer_signal_end(s_signal);
        }
    } else if (s_control.kind == CONTROL_RELEASE) {
        if (!abort_browsers()) rc = ESP_FAIL;
        else {
            s_retired = s_signal; s_signal = NULL;
            atomic_store(&s_active, false); atomic_store(&s_draining, false);
        }
    } else {
        // IP loss retires transport identities, without restarting the no-peer
        // grace interval or allocating a second catalog on reconnect.
        atomic_store(&s_active, false);
        if (!abort_browsers()) rc = ESP_FAIL;
        else if (s_signal) {
            kp_transfer_signal_init(s_signal, send_message, NULL);
            ++s_generation; if (!s_generation) ++s_generation;
        }
    }
    presence_publish_at(s_control.kind == CONTROL_DISCONNECT && rc == ESP_OK);
    s_control.result = rc;
    atomic_store(&s_control_state, 2);
    xSemaphoreGive(s_control_done);
}
static void control_cleanup(control_kind_t kind, esp_err_t rc) {
    if (kind == CONTROL_ACTIVATE && s_prepared) { free(s_prepared); s_prepared = NULL; }
    if (kind == CONTROL_GATE) {
        s_release_phase = rc == ESP_OK ? 2 : 0;
        s_release_grace = rc == ESP_OK && s_control.force;
    }
    if (kind == CONTROL_RELEASE && rc == ESP_OK) {
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
static esp_err_t control(control_kind_t kind, bool force, uint64_t zero_since) {
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
    s_control.kind = kind; s_control.force = force; s_control.zero_since_ms = zero_since;
    atomic_store(&s_control_state, 1);
    if (httpd_queue_work(s_http, control_work, NULL) != ESP_OK) {
        atomic_store(&s_control_state, 0); control_cleanup(kind, ESP_FAIL); return ESP_FAIL;
    }
    return control_finish();
}
esp_err_t kp_transfer_web_activate(uint64_t zero_since) {
    if (!s_http || atomic_load(&s_stopping) || s_release_phase) return ESP_ERR_INVALID_STATE;
    if (atomic_load(&s_active) && !atomic_load(&s_control_state)) return ESP_OK;
    return control(CONTROL_ACTIVATE, false, zero_since);
}
esp_err_t kp_transfer_web_deactivate(bool force) {
    if (!s_http) return ESP_OK;
    if (s_release_phase != 2) {
        s_release_phase = 1;
        esp_err_t rc = control(CONTROL_GATE, force, 0);
        if (rc != ESP_OK) return rc;
    }
    if (s_release_grace) { s_release_grace = false; vTaskDelay(pdMS_TO_TICKS(300)); }
    return control(CONTROL_RELEASE, force, 0);
}
esp_err_t kp_transfer_web_disconnect(void) {
    atomic_store(&s_link_up, false);
    return s_http ? control(CONTROL_DISCONNECT, false, 0) : ESP_OK;
}
esp_err_t kp_transfer_web_stop(void) {
    if (!s_http) return ESP_OK;
    atomic_store(&s_stopping, true); s_ready = false;
    esp_err_t rc = kp_transfer_web_deactivate(true);
    if (rc != ESP_OK) return rc;
    rc = httpd_stop(s_http);
    if (rc != ESP_OK) return rc;
    // HTTPD joined its task and closed all sockets. Nothing queued can refer
    // to this instance or its semaphore after ownership is released.
    s_http = NULL; s_wake = s_config_wake = NULL;
    atomic_store(&s_poll_queued, false); atomic_store(&s_link_up, false);
    vSemaphoreDelete(s_control_done); s_control_done = NULL;
    return ESP_OK;
}
esp_err_t kp_transfer_web_listen_start(const char *ip, kp_transfer_wake_t wake, kp_transfer_wake_t config_wake) {
    if (!ip || !*ip || strlen(ip) >= sizeof(s_ip)) return ESP_ERR_INVALID_ARG;
    if (s_http) {
        if (!s_ready || atomic_load(&s_stopping) || strcmp(ip, s_ip)) return ESP_ERR_INVALID_STATE;
        atomic_store(&s_link_up, true); return ESP_OK;
    }
    s_ready = false; s_wake = wake; s_config_wake = config_wake; s_release_phase = 0; s_release_grace = false;
    atomic_store(&s_control_state, 0);
    snprintf(s_ip, sizeof(s_ip), "%s", ip);
    atomic_store(&s_stopping, false); atomic_store(&s_poll_queued, false);
    atomic_store(&s_active, false); atomic_store(&s_draining, false); atomic_store(&s_link_up, true);
    portENTER_CRITICAL(&s_presence_lock); s_presence = (kp_transfer_presence_t){0}; portEXIT_CRITICAL(&s_presence_lock);
    memset(s_clients, 0, sizeof(s_clients));
    s_control_done = xSemaphoreCreateBinary();
    if (!s_control_done) return ESP_ERR_NO_MEM;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); config.max_open_sockets = HTTP_SOCKETS; config.max_uri_handlers = 7;
    config.backlog_conn = 1;
    config.stack_size = 6144; config.lru_purge_enable = false; config.recv_wait_timeout = 2; config.send_wait_timeout = 2;
    config.open_fn = open_session; config.close_fn = close_session;
    esp_err_t rc = httpd_start(&s_http, &config);
    if (rc != ESP_OK) { vSemaphoreDelete(s_control_done); s_control_done = NULL; return rc; }
    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=asset}, {.uri="/transfer", .method=HTTP_GET, .handler=asset},
        {.uri="/transfer.js", .method=HTTP_GET, .handler=asset}, {.uri="/transfer.css", .method=HTTP_GET, .handler=asset},
        {.uri="/transfer-protocol.js", .method=HTTP_GET, .handler=asset},
        {.uri="/config", .method=HTTP_GET, .handler=asset},
        {.uri="/transfer/ws", .method=HTTP_GET, .handler=websocket, .is_websocket=true, .ws_pre_handshake_cb=pre_handshake}
    };
    for (unsigned i = 0; i < sizeof(routes)/sizeof(routes[0]); ++i)
        if ((rc = httpd_register_uri_handler(s_http, &routes[i])) != ESP_OK) { kp_transfer_web_stop(); return rc; }
    s_ready = true;
    ESP_LOGI("transfer", "LAN listener ready; catalogs allocated on demand");
    return ESP_OK;
}
unsigned kp_transfer_web_peers(void) { kp_transfer_presence_t p; kp_transfer_web_presence(&p); return p.online; }
bool kp_transfer_web_running(void) { return s_http != NULL; }
bool kp_transfer_web_listening(void) { return s_http && s_ready && !atomic_load(&s_stopping) && atomic_load(&s_link_up); }
bool kp_transfer_web_ready(void) { return kp_transfer_web_listening() && atomic_load(&s_active); }
