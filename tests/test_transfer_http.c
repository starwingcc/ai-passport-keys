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
static unsigned routes,queue_count,sem_count,end_count,recv_count,stop_calls,shutdowns,http_starts;
static unsigned route_fault,stop_fault;static bool defer,timeout,queue_fault,abort_fault;
static struct { void (*fn)(void *);void *arg; } pending[16];static unsigned pending_count;
static uint64_t clock_ms;static const char *payload;static bool final=true;static int frame_type=HTTPD_WS_TYPE_TEXT;
static char response[80],last[KP_TRANSFER_MESSAGE+1];
static httpd_uri_t registered[7];static int handle_value,sem_value;
static int send_fault,shutdown_fd;
static unsigned aborts,diagnostics,wakes;static bool io_fault,server_owned;
static uint64_t wake_at;
static unsigned config_wakes;
static void config_wake(uint64_t now){(void)now;++config_wakes;}
static void wake(uint64_t now){++wakes;wake_at=now;}
static void run_next(void){assert(pending_count);void (*fn)(void *)=pending[0].fn;void *arg=pending[0].arg;memmove(pending,pending+1,--pending_count*sizeof(*pending));fn(arg);}
size_t heap_caps_get_free_size(unsigned caps){(void)caps;return 20000;}
size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 8000;}
void kp_transfer_diag_request(void){++diagnostics;}
void kp_socket_io_begin(kp_socket_io_t *io,uint32_t stall_ms,uint32_t total_ms){assert(stall_ms&&total_ms>=stall_ms);*io=(kp_socket_io_t){.started_ms=(uint32_t)clock_ms,.stall_ms=stall_ms,.total_ms=total_ms};}
int kp_socket_io_send(kp_socket_io_t *io,int fd,const char *data,size_t size){assert(fd>0&&data);if(io_fault){io->error=11;return -1;}io->sent+=size;return (int)size;}
bool kp_socket_io_abort(int fd){++aborts;if(abort_fault)return false;fake_shutdown(fd,SHUT_RDWR);return true;}
esp_err_t httpd_sess_set_send_override(httpd_handle_t h,int fd,httpd_send_func_t fn){assert(h==&handle_value&&fd>0&&fn==send_full);return ESP_OK;}
#define ASSET(name) const char name##_start[]="asset";const char name##_end[]="";
ASSET(transfer_html) ASSET(transfer_js) ASSET(transfer_css) ASSET(transfer_protocol) ASSET(config_wait)
int64_t esp_timer_get_time(void){return (int64_t)clock_ms*1000;}
int fake_close(int fd){(void)fd;return 0;}
int fake_shutdown(int fd,int how){assert(fd>0&&how==SHUT_RDWR);++shutdowns;shutdown_fd=fd;return 0;}
int httpd_ws_get_fd_info(httpd_handle_t h,int fd){assert(h==&handle_value);return client(fd)&&client(fd)->generation?HTTPD_WS_CLIENT_WEBSOCKET:HTTPD_WS_CLIENT_HTTP;}
SemaphoreHandle_t xSemaphoreCreateBinary(void){if(sem_fault)return NULL;++sem_count;return &sem_value;}
int xSemaphoreGive(SemaphoreHandle_t s){assert(s==&sem_value);++sem_value;return pdTRUE;}
int xSemaphoreTake(SemaphoreHandle_t s,TickType_t ms){assert(s==&sem_value&&(ms==6000||ms==0));if(timeout&&ms)return 0;while(ms&&pending_count&&!sem_value)run_next();if(!sem_value)return 0;--sem_value;return pdTRUE;}
void vSemaphoreDelete(SemaphoreHandle_t s){assert(s==&sem_value&&sem_count&&!pending_count);--sem_count;sem_value=0;}
void vTaskDelay(TickType_t ms){assert(ms==300);}
esp_err_t httpd_start(httpd_handle_t *out,const httpd_config_t *c){assert(!server_owned&&!s_signal&&!signal_allocation);assert(c->max_open_sockets==6&&c->max_uri_handlers==7&&!c->lru_purge_enable&&c->close_fn==close_session&&c->open_fn==open_session&&c->send_wait_timeout==2&&c->backlog_conn==1);if(start_fault)return ESP_FAIL;*out=&handle_value;routes=0;server_owned=true;++http_starts;return ESP_OK;}
esp_err_t httpd_stop(httpd_handle_t h){
    assert(h==&handle_value&&server_owned);++stop_calls;if(stop_fault)return ESP_FAIL;
    while(pending_count)run_next();
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used)close_session(h,s_clients[i].fd);
    server_owned=false;return ESP_OK;
}
esp_err_t httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t *uri){assert(h==&handle_value&&!strstr(uri->uri,"api"));if(route_fault&&routes==route_fault)return ESP_FAIL;registered[routes++]=*uri;return ESP_OK;}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *s){(void)r;snprintf(response,sizeof(response),"%s",s);return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *s){(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_send(httpd_req_t *r,const char *s,ssize_t n){(void)r;assert(s&&n>=0);return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *s){(void)r;assert(!strcmp(s,"text/html; charset=utf-8"));return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *k,const char *v){(void)r;assert(k&&v);if(!strcmp(k,"Location"))assert(!strcmp(v,"/transfer"));return ESP_OK;}
esp_err_t kp_web_asset(httpd_req_t *r,const char *a,const char *b,const char *type){(void)r;assert(a&&b&&type);return ESP_OK;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *key,char *out,size_t n){const char *value=!strcmp(key,"Host")?r->host:r->origin;if(!value||strlen(value)>=n)return ESP_FAIL;strcpy(out,value);return ESP_OK;}
int httpd_req_to_sockfd(httpd_req_t *r){return r->fd;}
esp_err_t httpd_ws_recv_frame(httpd_req_t *r,httpd_ws_frame_t *f,size_t n){(void)r;++recv_count;if(!n){f->len=strlen(payload);f->type=frame_type;f->final=final;return ESP_OK;}assert(n==strlen(payload));memcpy(f->payload,payload,n);return ESP_OK;}
esp_err_t httpd_ws_send_frame_async(httpd_handle_t h,int fd,httpd_ws_frame_t *f){assert(h==&handle_value&&fd>0&&f->len<=KP_TRANSFER_MESSAGE);memcpy(last,f->payload,f->len);last[f->len]=0;if(strstr(last,"\"end\""))++end_count;return send_fault==fd?ESP_FAIL:ESP_OK;}
esp_err_t httpd_sess_trigger_close(httpd_handle_t h,int fd){(void)h;(void)fd;assert(!"Never queue reusable session pointers");return ESP_FAIL;}
esp_err_t httpd_queue_work(httpd_handle_t h,void (*work)(void *),void *arg){assert(h==&handle_value);++queue_count;if(queue_fault)return ESP_FAIL;if(defer){assert(pending_count<16);pending[pending_count].fn=work;pending[pending_count++].arg=arg;}else work(arg);return ESP_OK;}
static httpd_req_t request(int fd){return (httpd_req_t){.host="192.168.1.11",.origin="http://192.168.1.11",.uri="/transfer",.fd=fd,.method=HTTP_GET};}
static void join(int fd){httpd_req_t r=request(fd);assert(open_session(s_http,fd)==ESP_OK);assert(pre_handshake(&r)==ESP_OK&&websocket(&r)==ESP_OK);r.method=HTTP_POST;payload="{\"type\":\"hello\",\"name\":\"browser\"}";assert(websocket(&r)==ESP_OK);}
static kp_transfer_presence_t presence(void){kp_transfer_presence_t p;kp_transfer_web_presence(&p);return p;}
int main(void){
    sem_fault=true;assert(kp_transfer_web_listen_start("192.168.1.11",wake,config_wake)==ESP_ERR_NO_MEM);sem_fault=false;
    start_fault=true;assert(kp_transfer_web_listen_start("192.168.1.11",wake,config_wake)==ESP_FAIL&&!sem_count&&!s_signal);start_fault=false;
    assert(kp_transfer_web_listen_start("192.168.1.11",wake,config_wake)==ESP_OK&&routes==7&&registered[6].is_websocket);
    assert(kp_transfer_web_listening()&&!kp_transfer_web_ready()&&!signal_allocation);
    // Webpage assets may span TCP retries with modem power saving. Keep the
    // existing configuration HTTP policy; short signaling frames stay bounded.
    httpd_req_t budget=request(60);budget.uri="/transfer.js";assert(open_session(s_http,60)==ESP_OK);
    assert(asset(&budget)==ESP_OK&&client(60)->io.stall_ms==5000&&client(60)->io.total_ms==30000);
    assert(send_message(NULL,60,"{}",2)&&client(60)->io.stall_ms==2000&&client(60)->io.total_ms==2000);
    close_session(s_http,60);
    httpd_req_t r=request(10);r.uri="/";assert(asset(&r)==ESP_OK&&!strcmp(response,"302 Found")&&wakes==1);
    r.uri="/transfer?reload=1";clock_ms=1;assert(asset(&r)==ESP_OK&&wakes==2&&wake_at==1);
    for(unsigned i=2;i<5;++i){r.uri=registered[i].uri;assert(asset(&r)==ESP_OK&&wakes==2);}
    r.uri="/unknown";assert(asset(&r)==ESP_OK&&wakes==2&&!strcmp(response,"404 Not Found"));
    r.uri="/config";assert(asset(&r)==ESP_OK&&config_wakes==1&&wakes==2&&!presence().allocated);
    r.uri="/config?retry=1";assert(asset(&r)==ESP_OK&&config_wakes==2&&wakes==2);
    r.host="invalid.test";assert(asset(&r)==ESP_OK&&config_wakes==2&&!strcmp(response,"403 Forbidden"));r.host="192.168.1.11";
    r.host="192.168.4.1";r.uri="/transfer";assert(asset(&r)==ESP_OK&&wakes==2&&!strcmp(response,"403 Forbidden"));r.host="192.168.1.11";
    assert(pre_handshake(&r)==ESP_FAIL&&!strcmp(response,"503 Service Unavailable")&&wakes==2);
    alloc_fault=true;assert(kp_transfer_web_activate(0)==ESP_ERR_NO_MEM&&kp_transfer_web_listening()&&!s_signal);alloc_fault=false;
    queue_fault=true;assert(kp_transfer_web_activate(0)==ESP_FAIL&&!signal_allocation&&signal_allocs==signal_frees);queue_fault=false;
    defer=true;timeout=true;assert(kp_transfer_web_activate(0)==ESP_ERR_TIMEOUT&&s_prepared&&signal_allocation&&!s_signal);
    unsigned queued=queue_count;timeout=false;assert(kp_transfer_web_activate(0)==ESP_OK&&queue_count==queued&&kp_transfer_web_ready());defer=false;
    void *owned=signal_allocation;assert(kp_transfer_web_activate(50)==ESP_OK&&signal_allocation==owned);
    // No join: resource reads/preconnects cannot extend the first grace window.
    clock_ms=59999;r.uri="/transfer.js";assert(asset(&r)==ESP_OK&&presence().zero_since_ms==0);
    assert(kp_transfer_web_deactivate(false)==ESP_ERR_INVALID_STATE&&s_signal);
    clock_ms=60000;assert(kp_transfer_web_deactivate(false)==ESP_OK&&!s_signal&&!signal_allocation&&kp_transfer_web_listening()&&http_starts==1);
    clock_ms=200000;assert(kp_transfer_web_activate(clock_ms)==ESP_OK);join(10);assert(presence().online==1);
    r=request(10);r.origin="http://evil.example";assert(pre_handshake(&r)==ESP_FAIL);r.origin="http://192.168.1.11";
    r.method=HTTP_POST;payload="{\"type\":\"ping\"}";
    unsigned before=recv_count;frame_type=HTTPD_WS_TYPE_BINARY;assert(websocket(&r)==ESP_FAIL&&recv_count==before+1);frame_type=HTTPD_WS_TYPE_TEXT;
    final=false;assert(websocket(&r)==ESP_FAIL);final=true;
    char huge[KP_TRANSFER_MESSAGE+2];memset(huge,'x',sizeof(huge)-1);huge[sizeof(huge)-1]=0;payload=huge;before=recv_count;assert(websocket(&r)==ESP_FAIL&&recv_count==before+1);
    payload="{\"type\":\"ping\"}";assert(websocket(&r)==ESP_OK);
    // Browser heartbeat expires at 120s; the 60s grace starts afterward.
    clock_ms=320001;poll_work((void *)(uintptr_t)s_generation);assert(presence().online==0&&presence().zero_since_ms==320000);
    clock_ms=379999;assert(kp_transfer_web_deactivate(false)==ESP_ERR_INVALID_STATE);
    clock_ms=380000;assert(kp_transfer_web_deactivate(false)==ESP_OK&&!signal_allocation);
    close_session(s_http,10);
    // A join racing a queued idle stop cancels release in the HTTP thread.
    clock_ms=600000;assert(kp_transfer_web_activate(clock_ms)==ESP_OK);
    clock_ms=660000;defer=true;timeout=true;assert(kp_transfer_web_deactivate(false)==ESP_ERR_TIMEOUT);
    join(10);timeout=false;assert(kp_transfer_web_deactivate(false)==ESP_ERR_INVALID_STATE&&kp_transfer_web_ready()&&presence().online==1);defer=false;
    // Capacity is four WebSocket reservations, but unjoined sockets aren't online.
    for(int fd=11;fd<14;++fd){r=request(fd);assert(open_session(s_http,fd)==ESP_OK&&pre_handshake(&r)==ESP_OK&&websocket(&r)==ESP_OK);}
    r=request(14);assert(pre_handshake(&r)==ESP_FAIL&&!strcmp(response,"503 Service Unavailable"));assert(presence().online==1);
    // Abort failure retains the catalog, the gate and one shutdown notification.
    unsigned ends=end_count;abort_fault=true;owned=signal_allocation;
    assert(kp_transfer_web_deactivate(true)==ESP_FAIL&&signal_allocation==owned&&!kp_transfer_web_ready()&&kp_transfer_web_listening());
    assert(end_count==ends+4);assert(websocket(&r)==ESP_FAIL);
    abort_fault=false;assert(kp_transfer_web_deactivate(true)==ESP_OK&&!signal_allocation&&end_count==ends+4);
    for(int fd=10;fd<14;++fd)close_session(s_http,fd);
    // Old poll/close callbacks must not operate on the next generation.
    clock_ms=900000;assert(kp_transfer_web_activate(clock_ms)==ESP_OK);join(10);uint32_t old=s_generation;
    clock_ms=900001;assert(kp_transfer_web_disconnect()==ESP_OK&&!kp_transfer_web_listening()&&s_signal&&presence().online==0&&presence().zero_since_ms==900001);
    assert(kp_transfer_web_listen_start("192.168.1.11",wake,config_wake)==ESP_OK&&kp_transfer_web_activate(presence().zero_since_ms)==ESP_OK);
    r=request(10);r.method=HTTP_POST;payload="{\"type\":\"hello\",\"name\":\"stale\"}";assert(websocket(&r)==ESP_FAIL);
    poll_work((void *)(uintptr_t)old);close_session(s_http,10);join(10);assert(presence().online==1);
    // Failed sends retire their socket without queued session pointers; fd reuse is clean.
    send_fault=10;assert(!send_message(NULL,10,"{}",2)&&client(10)->failed&&diagnostics&&aborts);send_fault=0;
    assert(send_full(s_http,10,"lost",4,0)==HTTPD_SOCK_ERR_FAIL);close_session(s_http,10);join(10);assert(send_full(s_http,10,"ok",2,0)==2);
    for(int fd=11;fd<16;++fd)assert(open_session(s_http,fd)==ESP_OK);clock_ms+=HTTP_IDLE_MS;
    unsigned closed=shutdowns;poll_work((void *)(uintptr_t)s_generation);assert(shutdowns==closed+1&&shutdown_fd!=10);
    for(int fd=11;fd<16;++fd)close_session(s_http,fd);
    assert(kp_transfer_web_deactivate(true)==ESP_OK);close_session(s_http,10);
    // Explicit departure is later than an already-expired neighbor. Its
    // zero-online anchor must be departure time, not that neighbor's deadline.
    clock_ms=1000000;assert(kp_transfer_web_activate(clock_ms)==ESP_OK);join(10);
    clock_ms=1060000;join(11);clock_ms=1130000;close_session(s_http,11);
    assert(presence().online==0&&presence().zero_since_ms==1130000);
    clock_ms+=60000;assert(kp_transfer_web_deactivate(false)==ESP_OK);close_session(s_http,10);
    // Repeated activation/release retains one server and balances catalog memory.
    for(unsigned i=0;i<100;++i){clock_ms+=200000;assert(kp_transfer_web_activate(clock_ms)==ESP_OK);clock_ms+=60000;assert(kp_transfer_web_deactivate(false)==ESP_OK);assert(!signal_allocation&&signal_allocs==signal_frees&&http_starts==1);}
    defer=true;kp_transfer_web_poll(clock_ms);assert(pending_count==1);stop_fault=1;
    assert(kp_transfer_web_stop()==ESP_FAIL&&kp_transfer_web_running()&&sem_count==1);stop_fault=0;
    assert(kp_transfer_web_stop()==ESP_OK&&!sem_count&&!pending_count&&!server_owned);defer=false;
    // Partial listener registration cannot leak or contend with configuration.
    route_fault=2;assert(kp_transfer_web_listen_start("192.168.1.11",wake,config_wake)==ESP_FAIL&&!kp_transfer_web_running());
    stop_fault=1;assert(kp_transfer_web_listen_start("192.168.1.11",wake,config_wake)==ESP_FAIL&&kp_transfer_web_running()&&!kp_transfer_web_listening());
    stop_fault=0;assert(kp_transfer_web_stop()==ESP_OK);route_fault=0;
    assert(signal_allocs==signal_frees&&!s_prepared&&!s_retired&&!signal_allocation&&!sem_count);
    puts("Transfer HTTP: single listener, valid-page wake, lazy catalogs, 120s + 60s timing, raced join, four slots, retained stop ownership, generation safety, reusable sockets and 100 balanced cycles: PASS");
}
