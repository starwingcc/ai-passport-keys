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
static atomic_uint s_peers;
static atomic_bool s_poll_queued, s_stopping;
static SemaphoreHandle_t s_end_done;
static atomic_uint s_end_state;
static bool s_ready;
#define HTTP_SOCKETS 6
#define HTTP_IDLE_MS 2000U
#define TRANSFER_SEND_MS 2000U
typedef struct {
    int fd;
    uint32_t opened;
    bool used, sending, failed;
    kp_socket_io_t io;
} client_t;
static client_t s_clients[HTTP_SOCKETS];
extern const char transfer_html_start[] __asm__("_binary_transfer_html_start"), transfer_html_end[] __asm__("_binary_transfer_html_end");
extern const char transfer_js_start[] __asm__("_binary_transfer_js_start"), transfer_js_end[] __asm__("_binary_transfer_js_end");
extern const char transfer_css_start[] __asm__("_binary_transfer_css_start"), transfer_css_end[] __asm__("_binary_transfer_css_end");
extern const char transfer_protocol_start[] __asm__("_binary_transfer_protocol_js_start"), transfer_protocol_end[] __asm__("_binary_transfer_protocol_js_end");
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static client_t *client(int fd) {
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used&&s_clients[i].fd==fd)return &s_clients[i];
    return NULL;
}
static void abort_session(int fd) {
    client_t *c=client(fd);
    if(c&&c->failed)return;
    if(c)c->failed=true;
    if(!kp_socket_io_abort(fd))ESP_LOGW("transfer","Cannot set abortive close fd=%d",fd);
}
static void begin_send(int fd) {
    client_t *c=client(fd);
    if(c){kp_socket_io_begin(&c->io,TRANSFER_SEND_MS,TRANSFER_SEND_MS);c->sending=true;}
}
static void end_send(int fd, const char *kind, esp_err_t result) {
    client_t *c=client(fd);
    if(!c)return;
    uint32_t elapsed=now_ms()-c->io.started_ms;
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
        s_clients[i]=(client_t){.used=true,.fd=fd,.opened=now_ms()};
        return httpd_sess_set_send_override(http,fd,send_full);
    }
    return ESP_FAIL;
}
static void close_session(httpd_handle_t http, int fd) {
    (void)http;
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used&&s_clients[i].fd==fd)s_clients[i].used=false;
    close(fd);
    for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)if(s_signal->peers[i].id&&s_signal->peers[i].fd==fd)
        ESP_LOGI("transfer","Browser session closed id=%u stopping=%d",(unsigned)s_signal->peers[i].id,atomic_load(&s_stopping));
    if(atomic_load(&s_stopping)) {
        for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)if(s_signal->peers[i].id&&s_signal->peers[i].fd==fd)memset(&s_signal->peers[i],0,sizeof(s_signal->peers[i]));
    } else kp_transfer_signal_close(s_signal,fd);
    atomic_store(&s_peers,kp_transfer_signal_count(s_signal));
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
    unsigned reserved=0;
    for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)reserved+=s_signal->peers[i].id!=0;
    if(reserved>=KP_TRANSFER_PEERS){fail(r,"503 Service Unavailable","Four browser sessions maximum");return ESP_FAIL;}
    return ESP_OK;
}
static esp_err_t websocket(httpd_req_t *r) {
    if(atomic_load(&s_stopping))return ESP_FAIL;
    int fd=httpd_req_to_sockfd(r);
    if(r->method==HTTP_GET)return kp_transfer_signal_open(s_signal,fd,now_ms())?ESP_OK:ESP_FAIL;
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
    atomic_store(&s_peers,kp_transfer_signal_count(s_signal));
    // Invalid messages terminate the offending session, so it cannot flood an
    // unbounded error queue. Content is never printed in logs.
    return ok?ESP_OK:ESP_FAIL;
}
static esp_err_t asset_body(httpd_req_t *r) {
    if(atomic_load(&s_stopping)||!valid_host(r,false))return fail(r,"403 Forbidden","Use the address on Passport");
    if(!strcmp(r->uri,"/")){httpd_resp_set_status(r,"302 Found");httpd_resp_set_hdr(r,"Location","/transfer");return httpd_resp_sendstr(r,"");}
    if(!strcmp(r->uri,"/transfer.js"))return kp_web_asset(r,transfer_js_start,transfer_js_end,"text/javascript; charset=utf-8");
    if(!strcmp(r->uri,"/transfer-protocol.js"))return kp_web_asset(r,transfer_protocol_start,transfer_protocol_end,"text/javascript; charset=utf-8");
    if(!strcmp(r->uri,"/transfer.css"))return kp_web_asset(r,transfer_css_start,transfer_css_end,"text/css; charset=utf-8");
    return kp_web_asset(r,transfer_html_start,transfer_html_end,"text/html; charset=utf-8");
}
static esp_err_t asset(httpd_req_t *r) {
    // Reuse browser HTTP connections. Actively closing every asset leaves TCP
    // TIME_WAIT state on Passport and exhausts heap across repeated refreshes.
    int fd=httpd_req_to_sockfd(r);
    begin_send(fd);
    esp_err_t rc=asset_body(r);
    end_send(fd,"asset",rc);
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used&&s_clients[i].fd==fd)s_clients[i].opened=now_ms();
    return rc;
}
static void poll_work(void *arg) {
    (void)arg;
    int fd;
    while((fd=kp_transfer_signal_expired(s_signal,now_ms()))>=0) {
        ESP_LOGW("transfer","Browser heartbeat expired fd=%d",fd);
        abort_session(fd);kp_transfer_signal_close(s_signal,fd);
    }
    // SO_RCVTIMEO cannot expire a preconnect waiting in select(). When all
    // slots are occupied, reclaim the oldest idle HTTP socket only. Global
    // LRU eviction would also evict live WebSockets during a refresh burst.
    unsigned occupied=0;int idle=-1;uint32_t age=HTTP_IDLE_MS-1;
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used){
        ++occupied;uint32_t elapsed=now_ms()-s_clients[i].opened;
        if(elapsed>age&&httpd_ws_get_fd_info(s_http,s_clients[i].fd)==HTTPD_WS_CLIENT_HTTP){idle=(int)i;age=elapsed;}
    }
    if(occupied==HTTP_SOCKETS&&idle>=0)abort_session(s_clients[idle].fd);
    atomic_store(&s_peers,kp_transfer_signal_count(s_signal));atomic_store(&s_poll_queued,false);
}
void kp_transfer_web_poll(uint32_t now) {
    (void)now;
    if(s_http&&!atomic_load(&s_stopping)&&!atomic_exchange(&s_poll_queued,true)) {
        if(httpd_queue_work(s_http,poll_work,NULL)!=ESP_OK)atomic_store(&s_poll_queued,false);
    }
}
static void end_work(void *arg) {(void)arg;kp_transfer_signal_end(s_signal);atomic_store(&s_end_state,2);xSemaphoreGive(s_end_done);}
esp_err_t kp_transfer_web_stop(void) {
    if(!s_http)return ESP_OK;
    atomic_store(&s_stopping,true); s_ready=false;
    // One queued shutdown and one periodic work item are the only background
    // queue users. Do not free handles when a stop failure retains ownership.
    if(atomic_load(&s_end_state)==0) {
        atomic_store(&s_end_state,1);
        if(httpd_queue_work(s_http,end_work,NULL)!=ESP_OK){atomic_store(&s_end_state,0);return ESP_FAIL;}
    }
    if(atomic_load(&s_end_state)==1 && xSemaphoreTake(s_end_done,pdMS_TO_TICKS(6000))!=pdTRUE)return ESP_ERR_TIMEOUT;
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_err_t rc=httpd_stop(s_http);if(rc!=ESP_OK)return rc;
    s_http=NULL;atomic_store(&s_peers,0);atomic_store(&s_poll_queued,false);
    vSemaphoreDelete(s_end_done);s_end_done=NULL;
    // httpd_stop has joined the worker and closed every session; callbacks can
    // no longer access the catalogs. Failed stops retain them for a retry.
    free(s_signal);s_signal=NULL;return ESP_OK;
}
esp_err_t kp_transfer_web_start(const char *ip) {
    if(s_http)return !atomic_load(&s_stopping)&&!strcmp(ip,s_ip)?ESP_OK:ESP_ERR_INVALID_STATE;
    if(!ip||!*ip||strlen(ip)>=sizeof(s_ip))return ESP_ERR_INVALID_ARG;
    s_ready=false;atomic_store(&s_end_state,0);
    snprintf(s_ip,sizeof(s_ip),"%s",ip);atomic_store(&s_stopping,false);atomic_store(&s_poll_queued,false);
    s_signal=malloc(sizeof(*s_signal));if(!s_signal)return ESP_ERR_NO_MEM;
    kp_transfer_signal_init(s_signal,send_message,NULL);
    memset(s_clients,0,sizeof(s_clients));
    s_end_done=xSemaphoreCreateBinary();
    if(!s_end_done){free(s_signal);s_signal=NULL;return ESP_ERR_NO_MEM;}
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.max_open_sockets=HTTP_SOCKETS;config.max_uri_handlers=6;
    // lwIP allocates TCP state and queues incoming data before accept(). These
    // connections are additional to max_open_sockets (the IDF default is five).
    config.backlog_conn=1;
    config.stack_size=6144;config.lru_purge_enable=false;config.recv_wait_timeout=2;config.send_wait_timeout=2;config.open_fn=open_session;config.close_fn=close_session;
    esp_err_t rc=httpd_start(&s_http,&config);
    if(rc!=ESP_OK){vSemaphoreDelete(s_end_done);s_end_done=NULL;free(s_signal);s_signal=NULL;return rc;}
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=asset}, {.uri="/transfer",.method=HTTP_GET,.handler=asset},
        {.uri="/transfer.js",.method=HTTP_GET,.handler=asset}, {.uri="/transfer.css",.method=HTTP_GET,.handler=asset},
        {.uri="/transfer-protocol.js",.method=HTTP_GET,.handler=asset},
        {.uri="/transfer/ws",.method=HTTP_GET,.handler=websocket,.is_websocket=true,.ws_pre_handshake_cb=pre_handshake}
    };
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i)if((rc=httpd_register_uri_handler(s_http,&routes[i]))!=ESP_OK){kp_transfer_web_stop();return rc;}
    s_ready=true;
    ESP_LOGI("transfer","Service ready; maximum four browser sessions");return ESP_OK;
}
unsigned kp_transfer_web_peers(void) {return atomic_load(&s_peers);}
bool kp_transfer_web_running(void) {return s_http!=NULL;}

bool kp_transfer_web_ready(void) {return s_http && s_ready && !atomic_load(&s_stopping);}
