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
static httpd_uri_t registered[10];static int handle_value,sem_value;
static int send_fault,shutdown_fd;
static unsigned aborts,diagnostics;static bool io_fault,server_owned;
static void run_next(void){assert(pending_count);void (*fn)(void *)=pending[0].fn;void *arg=pending[0].arg;memmove(pending,pending+1,--pending_count*sizeof(*pending));fn(arg);}
size_t heap_caps_get_free_size(unsigned caps){(void)caps;return 20000;}
size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 8000;}
void kp_transfer_diag_request(void){++diagnostics;}
void kp_socket_io_begin(kp_socket_io_t *io,uint32_t stall_ms,uint32_t total_ms){assert(stall_ms&&total_ms>=stall_ms);*io=(kp_socket_io_t){.started_ms=(uint32_t)clock_ms,.stall_ms=stall_ms,.total_ms=total_ms};}
int kp_socket_io_send(kp_socket_io_t *io,int fd,const char *data,size_t size){assert(fd>0&&data);if(io_fault){io->error=11;return -1;}io->sent+=size;return (int)size;}
bool kp_socket_io_abort(int fd){++aborts;if(abort_fault)return false;fake_shutdown(fd,SHUT_RDWR);return true;}
esp_err_t httpd_sess_set_send_override(httpd_handle_t h,int fd,httpd_send_func_t fn){assert(h==&handle_value&&fd>0&&fn==send_full);return ESP_OK;}
#define ASSET(name) const char name##_start[]="asset";const char name##_end[]="";
ASSET(transfer_html) ASSET(transfer_js) ASSET(transfer_css) ASSET(transfer_protocol)
const char config_wait_start[] __asm__("_binary_config_wait_html_start")="asset";
const char config_wait_end[] __asm__("_binary_config_wait_html_end")="";
int64_t esp_timer_get_time(void){return (int64_t)clock_ms*1000;}
int fake_close(int fd){(void)fd;return 0;}
int fake_shutdown(int fd,int how){assert(fd>0&&how==SHUT_RDWR);++shutdowns;shutdown_fd=fd;return 0;}
int httpd_ws_get_fd_info(httpd_handle_t h,int fd){assert(h==&handle_value);return client(fd)&&client(fd)->generation?HTTPD_WS_CLIENT_WEBSOCKET:HTTPD_WS_CLIENT_HTTP;}
SemaphoreHandle_t xSemaphoreCreateBinary(void){if(sem_fault)return NULL;++sem_count;return &sem_value;}
int xSemaphoreGive(SemaphoreHandle_t s){assert(s==&sem_value);++sem_value;return pdTRUE;}
int xSemaphoreTake(SemaphoreHandle_t s,TickType_t ms){assert(s==&sem_value&&(ms==6000||ms==0));if(timeout&&ms)return 0;while(ms&&pending_count&&!sem_value)run_next();if(!sem_value)return 0;--sem_value;return pdTRUE;}
void vSemaphoreDelete(SemaphoreHandle_t s){assert(s==&sem_value&&sem_count&&!pending_count);--sem_count;sem_value=0;}
void vTaskDelay(TickType_t ms){assert(ms==300);}
esp_err_t httpd_start(httpd_handle_t *out,const httpd_config_t *c){assert(!server_owned&&!s_signal&&!signal_allocation);assert(c->max_open_sockets==6&&c->max_uri_handlers==10&&!c->lru_purge_enable&&c->close_fn==close_session&&c->open_fn==open_session&&c->send_wait_timeout==2&&c->backlog_conn==1);if(start_fault)return ESP_FAIL;*out=&handle_value;routes=0;server_owned=true;++http_starts;return ESP_OK;}
esp_err_t httpd_stop(httpd_handle_t h){
    assert(h==&handle_value&&server_owned);++stop_calls;if(stop_fault)return ESP_FAIL;
    while(pending_count)run_next();
    for(unsigned i=0;i<HTTP_SOCKETS;++i)if(s_clients[i].used)close_session(h,s_clients[i].fd);
    server_owned=false;return ESP_OK;
}
esp_err_t httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t *uri){assert(h==&handle_value);if(route_fault&&routes==route_fault)return ESP_FAIL;registered[routes++]=*uri;return ESP_OK;}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *s){(void)r;snprintf(response,sizeof(response),"%s",s);return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *s){(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_send(httpd_req_t *r,const char *s,ssize_t n){(void)r;assert(s&&n>=0);return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *s){(void)r;assert(!strcmp(s,"text/html; charset=utf-8")||!strcmp(s,"application/json"));return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *k,const char *v){(void)r;assert(k&&v);if(!strcmp(k,"Location"))assert(!strcmp(v,"/transfer"));return ESP_OK;}
esp_err_t kp_web_asset(httpd_req_t *r,const char *a,const char *b,const char *type){(void)r;assert(a&&b&&type);return ESP_OK;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *key,char *out,size_t n){const char *value=!strcmp(key,"Host")?r->host:r->origin;if(!value||strlen(value)>=n)return ESP_FAIL;strcpy(out,value);return ESP_OK;}
int httpd_req_to_sockfd(httpd_req_t *r){return r->fd;}
esp_err_t httpd_ws_recv_frame(httpd_req_t *r,httpd_ws_frame_t *f,size_t n){(void)r;++recv_count;if(!n){f->len=strlen(payload);f->type=frame_type;f->final=final;return ESP_OK;}assert(n==strlen(payload));memcpy(f->payload,payload,n);return ESP_OK;}
esp_err_t httpd_ws_send_frame_async(httpd_handle_t h,int fd,httpd_ws_frame_t *f){assert(h==&handle_value&&fd>0&&f->len<=KP_TRANSFER_MESSAGE);memcpy(last,f->payload,f->len);last[f->len]=0;if(strstr(last,"\"end\""))++end_count;return send_fault==fd?ESP_FAIL:ESP_OK;}
esp_err_t httpd_sess_trigger_close(httpd_handle_t h,int fd){(void)h;(void)fd;assert(!"Never queue reusable session pointers");return ESP_FAIL;}
esp_err_t httpd_queue_work(httpd_handle_t h,void (*work)(void *),void *arg){assert(h==&handle_value);++queue_count;if(queue_fault)return ESP_FAIL;if(defer){assert(pending_count<16);pending[pending_count].fn=work;pending[pending_count++].arg=arg;}else work(arg);return ESP_OK;}
static httpd_req_t request(int fd){return (httpd_req_t){.host="192.168.1.11",.origin="http://192.168.1.11",.uri="/transfer",.fd=fd,.method=HTTP_GET};}
static uint32_t first_lease;
int httpd_req_recv(httpd_req_t *r,char *out,size_t n){size_t left=r->content_len-r->received;if(n>left)n=left;memcpy(out,r->body+r->received,n);r->received+=n;return (int)n;}
static uint32_t activate(uint64_t now) {
    uint32_t gen;first_lease=kp_session_open(KP_SESSION_TRANSFER,now,&gen);assert(first_lease);
    assert(kp_transfer_web_activate(now)==ESP_OK);kp_session_ready(KP_SESSION_TRANSFER,gen,true);return gen;
}
static void join(int fd){
    uint32_t gen=s_session_generation,id=first_lease;if(!id)id=kp_session_open(KP_SESSION_TRANSFER,clock_ms,&gen);first_lease=0;assert(id);
    httpd_req_t r=request(fd);assert(open_session(s_http,fd)==ESP_OK);assert(pre_handshake(&r)==ESP_OK&&websocket(&r)==ESP_OK);
    char hello[128];snprintf(hello,sizeof(hello),"{\"type\":\"hello\",\"name\":\"browser\",\"generation\":%u,\"lease\":%u}",gen,id);
    r.method=HTTP_POST;payload=hello;assert(websocket(&r)==ESP_OK);
}
static kp_transfer_presence_t presence(void){kp_transfer_presence_t p;kp_transfer_web_presence(&p);return p;}
int main(void){
    kp_session_reset();
    sem_fault=true;assert(kp_transfer_web_listen_start("192.168.1.11")==ESP_ERR_NO_MEM);sem_fault=false;
    start_fault=true;assert(kp_transfer_web_listen_start("192.168.1.11")==ESP_FAIL&&!sem_count);start_fault=false;
    assert(kp_transfer_web_listen_start("192.168.1.11")==ESP_OK&&routes==10&&registered[9].is_websocket);
    assert(kp_transfer_web_listening()&&!kp_transfer_web_ready()&&!signal_allocation);
    httpd_req_t r=request(60);assert(open_session(s_http,60)==ESP_OK);r.uri="/transfer.js";
    assert(asset(&r)==ESP_OK&&client(60)->io.total_ms==30000);
    assert(send_message(NULL,60,"{}",2)&&client(60)->io.total_ms==2000);close_session(s_http,60);
    r=request(10);r.uri="/";assert(asset(&r)==ESP_OK&&!strcmp(response,"302 Found")&&!signal_allocation);
    r.uri="/config";assert(asset(&r)==ESP_OK&&!signal_allocation);
    kp_session_view_t idle;kp_session_get(&idle);assert(idle.mode==KP_SESSION_LISTEN&&!idle.joining);
    r.host="invalid.test";assert(asset(&r)==ESP_OK&&!strcmp(response,"403 Forbidden"));r=request(10);
    assert(pre_handshake(&r)==ESP_FAIL&&!strcmp(response,"503 Service Unavailable"));
    uint32_t generation;first_lease=kp_session_open(KP_SESSION_TRANSFER,0,&generation);
    alloc_fault=true;assert(kp_transfer_web_activate(0)==ESP_ERR_NO_MEM);alloc_fault=false;
    queue_fault=true;assert(kp_transfer_web_activate(0)==ESP_FAIL&&!signal_allocation);queue_fault=false;
    defer=true;timeout=true;assert(kp_transfer_web_activate(0)==ESP_ERR_TIMEOUT&&s_prepared);
    timeout=false;assert(kp_transfer_web_activate(0)==ESP_OK);defer=false;
    clock_ms=29999;assert(kp_transfer_web_deactivate(false)==ESP_ERR_INVALID_STATE);
    clock_ms=30000;kp_session_poll(clock_ms);assert(kp_transfer_web_deactivate(false)==ESP_OK&&!signal_allocation&&http_starts==1);
    clock_ms=40000;activate(clock_ms);join(10);assert(presence().online==1);
    r=request(10);r.origin="http://evil.test";assert(pre_handshake(&r)==ESP_FAIL);r.origin="http://192.168.1.11";r.method=HTTP_POST;
    payload="{\"type\":\"ping\"}";frame_type=HTTPD_WS_TYPE_BINARY;assert(websocket(&r)==ESP_FAIL);frame_type=HTTPD_WS_TYPE_TEXT;
    final=false;assert(websocket(&r)==ESP_FAIL);final=true;assert(websocket(&r)==ESP_OK);
    clock_ms=159999;kp_session_poll(clock_ms);assert(kp_transfer_web_ready());
    ++clock_ms;kp_session_poll(clock_ms);poll_work((void *)(uintptr_t)s_generation);assert(!kp_transfer_web_ready());
    assert(kp_transfer_web_deactivate(false)==ESP_OK&&!signal_allocation);close_session(s_http,10);
    clock_ms=200000;activate(clock_ms);join(10);join(11);
    close_session(s_http,10);assert(kp_transfer_web_ready()&&presence().online==1);
    close_session(s_http,11);assert(!kp_transfer_web_ready());assert(kp_transfer_web_deactivate(false)==ESP_OK&&!signal_allocation);
    clock_ms=300000;activate(clock_ms);join(10);
    for(int fd=11;fd<14;++fd){r=request(fd);assert(open_session(s_http,fd)==ESP_OK&&pre_handshake(&r)==ESP_OK&&websocket(&r)==ESP_OK);}
    r=request(14);assert(pre_handshake(&r)==ESP_FAIL);assert(presence().online==1);
    unsigned ends=end_count;abort_fault=true;void *owned=signal_allocation;
    assert(kp_transfer_web_deactivate(true)==ESP_FAIL&&signal_allocation==owned&&!kp_transfer_web_ready());assert(end_count==ends+4);
    abort_fault=false;assert(kp_transfer_web_deactivate(true)==ESP_OK&&!signal_allocation&&end_count==ends+4);
    for(int fd=10;fd<14;++fd)close_session(s_http,fd);
    kp_session_end(KP_SESSION_TRANSFER);
    // Switching generation immediately rejects the old browser, before teardown.
    clock_ms=400000;activate(clock_ms);join(10);
    uint32_t config_generation;assert(kp_session_open(KP_SESSION_CONFIG,clock_ms,&config_generation));
    r=request(10);r.method=HTTP_POST;payload="{\"type\":\"ping\"}";assert(websocket(&r)==ESP_FAIL);
    assert(kp_transfer_web_deactivate(true)==ESP_OK);close_session(s_http,10);
    // Repeated catalog cycles retain one listener and balance all allocations.
    for(unsigned i=0;i<100;++i){clock_ms=500000+i*40000;activate(clock_ms);join(10);close_session(s_http,10);assert(kp_transfer_web_deactivate(false)==ESP_OK);}
    assert(http_starts==1&&signal_allocs==signal_frees);
    stop_fault=1;assert(kp_transfer_web_stop()==ESP_FAIL&&kp_transfer_web_running());stop_fault=0;assert(kp_transfer_web_stop()==ESP_OK&&!server_owned&&!sem_count);
    route_fault=2;assert(kp_transfer_web_listen_start("192.168.1.11")==ESP_FAIL&&!kp_transfer_web_running());route_fault=0;
    assert(signal_allocs==signal_frees&&!s_prepared&&!s_retired);
    puts("Transfer HTTP: explicit admission, immediate leave, 120s expiry, stale-generation gate, protocol checks, forced teardown ownership and 100 balanced catalog cycles: PASS");
}
