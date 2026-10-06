#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake_web.h"
#include "keypad_transfer_signal.h"
static void *signal_allocation;
static unsigned signal_allocs,signal_frees;
static bool alloc_fault,sem_fault,start_fault;
static void *transfer_malloc(size_t size){
    if(size!=sizeof(kp_transfer_signal_t))return malloc(size);
    assert(!signal_allocation);
    if(alloc_fault)return NULL;
    signal_allocation=malloc(size);assert(signal_allocation);++signal_allocs;
    return signal_allocation;
}
static void transfer_free(void *p){
    if(p&&p==signal_allocation){signal_allocation=NULL;++signal_frees;}
    free(p);
}
static void test_log(const char *tag,const char *format,...){(void)tag;(void)format;}
#undef ESP_LOGW
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define close fake_close
#define shutdown fake_shutdown
#define malloc transfer_malloc
#define free transfer_free
#include "../main/keyboard/keypad_transfer_web.c"
#undef malloc
#undef free
#undef close
#undef shutdown
static unsigned routes,queue_count,sem_count,end_count,recv_count,triggered,stop_calls,shutdowns;
static unsigned route_fault,stop_fault;static bool defer,timeout;
static void (*pending_work)(void *);static void *pending_arg;
static uint32_t clock_ms;static const char *payload;static bool final=true;static int frame_type=HTTPD_WS_TYPE_TEXT;
static char response[80],last[KP_TRANSFER_MESSAGE+1];
static httpd_uri_t registered[6];static int handle_value,sem_value;
static int send_fault,shutdown_fd;
static unsigned aborts,diagnostics;static bool io_fault;
size_t heap_caps_get_free_size(unsigned caps){(void)caps;return 20000;}
size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 8000;}
void kp_transfer_diag_request(void){++diagnostics;}
void kp_socket_io_begin(kp_socket_io_t *io,uint32_t stall_ms,uint32_t total_ms){assert(stall_ms==TRANSFER_SEND_MS&&total_ms==TRANSFER_SEND_MS);*io=(kp_socket_io_t){.started_ms=clock_ms};}
int kp_socket_io_send(kp_socket_io_t *io,int fd,const char *data,size_t size){assert(fd>0&&data);if(io_fault){io->error=11;return -1;}io->sent+=size;return (int)size;}
bool kp_socket_io_abort(int fd){++aborts;fake_shutdown(fd,SHUT_RDWR);return true;}
esp_err_t httpd_sess_set_send_override(httpd_handle_t h,int fd,httpd_send_func_t fn){assert(h==&handle_value&&fd>0&&fn==send_full);return ESP_OK;}
#define ASSET(name) const char name##_start[]="asset";const char name##_end[]="";
ASSET(transfer_html) ASSET(transfer_js) ASSET(transfer_css) ASSET(transfer_protocol)
int64_t esp_timer_get_time(void){return (int64_t)clock_ms*1000;}
int fake_close(int fd){(void)fd;return 0;}
int fake_shutdown(int fd,int how){assert(fd>0&&how==SHUT_RDWR);++shutdowns;shutdown_fd=fd;return 0;}
int httpd_ws_get_fd_info(httpd_handle_t h,int fd){assert(h==&handle_value);if(fd==10)return HTTPD_WS_CLIENT_WEBSOCKET;return HTTPD_WS_CLIENT_HTTP;}
SemaphoreHandle_t xSemaphoreCreateBinary(void){if(sem_fault)return NULL;++sem_count;return &sem_value;}
int xSemaphoreGive(SemaphoreHandle_t s){assert(s==&sem_value);++sem_value;return pdTRUE;}
int xSemaphoreTake(SemaphoreHandle_t s,TickType_t ms){assert(s==&sem_value&&ms==6000);if(timeout)return 0;if(pending_work){void (*work)(void *)=pending_work;pending_work=NULL;work(pending_arg);}assert(sem_value);--sem_value;return pdTRUE;}
void vSemaphoreDelete(SemaphoreHandle_t s){assert(s==&sem_value&&sem_count);--sem_count;sem_value=0;}
void vTaskDelay(TickType_t ms){assert(ms==300);}
esp_err_t httpd_start(httpd_handle_t *out,const httpd_config_t *c){assert(c->max_open_sockets==6&&c->max_uri_handlers==6&&!c->lru_purge_enable&&c->close_fn==close_session&&c->open_fn==open_session&&c->send_wait_timeout==2&&c->backlog_conn==1);assert(s_signal&&signal_allocation==s_signal);if(start_fault)return ESP_FAIL;*out=&handle_value;routes=0;return ESP_OK;}
esp_err_t httpd_stop(httpd_handle_t h){
    assert(h==&handle_value&&s_signal&&signal_allocation==s_signal);++stop_calls;
    if(stop_fault)return ESP_FAIL;
    // Real HTTPD closes sessions before joining its worker. Catalog storage
    // must remain alive throughout those callbacks.
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used)close_session(h,s_clients[i].fd);
    return ESP_OK;
}
esp_err_t httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t *uri){assert(h==&handle_value&&!strstr(uri->uri,"api"));if(route_fault&&routes==route_fault)return ESP_FAIL;registered[routes++]=*uri;return ESP_OK;}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *s){(void)r;snprintf(response,sizeof(response),"%s",s);return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *s){(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *k,const char *v){(void)r;assert((!strcmp(k,"Location")&&!strcmp(v,"/transfer"))||(!strcmp(k,"Connection")&&!strcmp(v,"close")));return ESP_OK;}
esp_err_t kp_web_asset(httpd_req_t *r,const char *a,const char *b,const char *type){(void)r;assert(a&&b&&type);return ESP_OK;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *key,char *out,size_t n){const char *value=!strcmp(key,"Host")?r->host:r->origin;if(!value||strlen(value)>=n)return ESP_FAIL;strcpy(out,value);return ESP_OK;}
int httpd_req_to_sockfd(httpd_req_t *r){return r->fd;}
esp_err_t httpd_ws_recv_frame(httpd_req_t *r,httpd_ws_frame_t *f,size_t n){(void)r;++recv_count;if(!n){f->len=strlen(payload);f->type=frame_type;f->final=final;return ESP_OK;}assert(n==strlen(payload));memcpy(f->payload,payload,n);return ESP_OK;}
esp_err_t httpd_ws_send_frame_async(httpd_handle_t h,int fd,httpd_ws_frame_t *f){assert(h==&handle_value&&fd>0&&f->len<=KP_TRANSFER_MESSAGE);memcpy(last,f->payload,f->len);last[f->len]=0;if(strstr(last,"\"end\""))++end_count;return send_fault==fd?ESP_FAIL:ESP_OK;}
esp_err_t httpd_sess_trigger_close(httpd_handle_t h,int fd){assert(h==&handle_value&&fd>0);++triggered;return ESP_OK;}
esp_err_t httpd_queue_work(httpd_handle_t h,void (*work)(void *),void *arg){assert(h==&handle_value);++queue_count;if(defer){assert(!pending_work);pending_work=work;pending_arg=arg;}else work(arg);return ESP_OK;}
int main(void){
    // No catalogs remain allocated while the transfer service is inactive,
    // including partial starts and retryable shutdown failures.
    assert(!s_signal&&!signal_allocation);
    alloc_fault=true;assert(kp_transfer_web_start("192.168.1.50")==ESP_ERR_NO_MEM);
    assert(!s_signal&&!s_http&&!sem_count&&!signal_allocation);alloc_fault=false;
    sem_fault=true;assert(kp_transfer_web_start("192.168.1.50")==ESP_ERR_NO_MEM);
    assert(!s_signal&&!s_http&&!sem_count&&!signal_allocation);sem_fault=false;
    start_fault=true;assert(kp_transfer_web_start("192.168.1.50")==ESP_FAIL);
    assert(!s_signal&&!s_http&&!sem_count&&!signal_allocation);start_fault=false;
    assert(kp_transfer_web_start("192.168.1.50")==ESP_OK&&routes==6&&kp_transfer_web_ready());
    void *owned=signal_allocation;unsigned allocations=signal_allocs;
    assert(kp_transfer_web_start("192.168.1.50")==ESP_OK&&signal_allocs==allocations);
    httpd_req_t r={.host="192.168.1.50",.origin="http://192.168.1.50",.uri="/",.fd=10,.method=HTTP_GET};
    assert(asset(&r)==ESP_OK&&!strcmp(response,"302 Found"));assert(pre_handshake(&r)==ESP_OK&&registered[5].is_websocket);
    r.origin="http://evil.example";assert(pre_handshake(&r)==ESP_FAIL&&!strcmp(response,"403 Forbidden"));r.origin="http://192.168.1.50";
    r.host="192.168.4.1";assert(pre_handshake(&r)==ESP_FAIL&&asset(&r)==ESP_OK);assert(!strcmp(response,"403 Forbidden"));r.host="192.168.1.50";
    assert(open_session(s_http,r.fd)==ESP_OK);assert(websocket(&r)==ESP_OK&&kp_transfer_web_peers()==0);r.method=HTTP_POST;payload="{\"type\":\"hello\",\"name\":\"phone\"}";assert(websocket(&r)==ESP_OK&&kp_transfer_web_peers()==1);
    unsigned before=recv_count;frame_type=HTTPD_WS_TYPE_BINARY;assert(websocket(&r)==ESP_FAIL&&recv_count==before+1);frame_type=HTTPD_WS_TYPE_TEXT;
    final=false;before=recv_count;assert(websocket(&r)==ESP_FAIL&&recv_count==before+1);final=true;
    char huge[KP_TRANSFER_MESSAGE+2];memset(huge,'x',sizeof(huge)-1);huge[sizeof(huge)-1]=0;payload=huge;before=recv_count;assert(websocket(&r)==ESP_FAIL&&recv_count==before+1);
    defer=true;before=queue_count;for(unsigned i=0;i<100;++i)kp_transfer_web_poll(0);assert(queue_count==before+1&&pending_work);
    void (*poll)(void *)=pending_work;pending_work=NULL;clock_ms=KP_TRANSFER_IDLE_MS;unsigned closed=shutdowns;poll(NULL);assert(!kp_transfer_web_peers()&&shutdowns==closed+1&&triggered==0);
    close_session(s_http,10);assert(open_session(s_http,10)==ESP_OK);
    r.method=HTTP_GET;assert(websocket(&r)==ESP_OK);r.method=HTTP_POST;payload="{\"type\":\"hello\",\"name\":\"pc\"}";assert(websocket(&r)==ESP_OK);
    // Failed sends do not enqueue closures holding reusable session pointers.
    unsigned queued=queue_count;send_fault=10;assert(!send_message(NULL,10,"{}",2));send_fault=0;
    assert(queue_count==queued&&shutdown_fd==10&&triggered==0);
    assert(client(10)->failed&&diagnostics&&aborts);
    assert(send_full(s_http,10,"lost",4,0)==HTTPD_SOCK_ERR_FAIL);
    close_session(s_http,10);assert(open_session(s_http,10)==ESP_OK);
    assert(send_full(s_http,10,"ok",2,0)==2); // reused fd has no stale failure
    r.method=HTTP_GET;assert(websocket(&r)==ESP_OK);r.method=HTTP_POST;assert(websocket(&r)==ESP_OK);
    // Idle preconnects are reclaimed, but active WebSocket descriptors survive.
    for(int fd=11;fd<16;++fd)assert(open_session(s_http,fd)==ESP_OK);clock_ms+=HTTP_IDLE_MS;
    closed=shutdowns;poll_work(NULL);assert(shutdowns==closed+1&&shutdown_fd!=10);
    for(int fd=11;fd<16;++fd)close_session(s_http,fd);
    assert(open_session(s_http,11)==ESP_OK);
    clock_ms+=HTTP_IDLE_MS-1;closed=shutdowns;poll_work(NULL);assert(shutdowns==closed);
    timeout=true;before=queue_count;assert(kp_transfer_web_stop()==ESP_ERR_TIMEOUT&&kp_transfer_web_running()&&!kp_transfer_web_ready());assert(queue_count==before+1);
    assert(signal_allocation==owned&&s_signal==owned);
    timeout=false;stop_fault=1;assert(kp_transfer_web_stop()==ESP_FAIL&&kp_transfer_web_running());assert(queue_count==before+1&&end_count==1);
    assert(signal_allocation==owned&&s_signal==owned);
    stop_fault=0;assert(kp_transfer_web_stop()==ESP_OK&&!kp_transfer_web_running()&&!sem_count);assert(queue_count==before+1);
    assert(!s_signal&&!signal_allocation&&signal_allocs==signal_frees);
    defer=false;route_fault=2;assert(kp_transfer_web_start("192.168.1.60")==ESP_FAIL&&!kp_transfer_web_running()&&!sem_count);
    assert(!s_signal&&!signal_allocation&&signal_allocs==signal_frees);
    stop_fault=1;assert(kp_transfer_web_start("192.168.1.60")==ESP_FAIL&&kp_transfer_web_running()&&!kp_transfer_web_ready());
    assert(s_signal&&signal_allocation==s_signal&&sem_count==1);
    stop_fault=0;assert(kp_transfer_web_stop()==ESP_OK&&!s_signal&&!signal_allocation);
    route_fault=0;assert(kp_transfer_web_start("192.168.1.60")==ESP_OK);assert(kp_transfer_web_stop()==ESP_OK);
    assert(!s_signal&&!signal_allocation&&signal_allocs==signal_frees&&!sem_count);
    puts("Transfer HTTP: isolated routes, Host/Origin, frame bounds, reusable assets, selective idle preconnect expiry, no queued stale closes, shutdown notification, catalog allocation lifetime and retained failed-stop ownership: PASS");
}
