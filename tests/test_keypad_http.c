#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake_web.h"
#include "keypad_model.h"

static bool fail_alloc;
static unsigned allocations;
static void *web_malloc(size_t size) {
    ++allocations;
    assert(size == KP_CONFIG_BYTES);
    return fail_alloc ? NULL : malloc(size);
}

// Exercise the actual embedded files and production handlers. A constrained
// fake transport rejects writes over 512 bytes, reproducing a large-send limit.
#define malloc web_malloc
#include "../main/keyboard/keypad_web.c"
#undef malloc
__asm__(".data\n"
        ".globl _binary_index_html_start\n_binary_index_html_start:\n"
        ".incbin \"main/web/index.html\"\n.byte 0\n"
        ".globl _binary_index_html_end\n_binary_index_html_end:\n"
        ".globl _binary_app_css_start\n_binary_app_css_start:\n"
        ".incbin \"main/web/app.css\"\n.byte 0\n"
        ".globl _binary_app_css_end\n_binary_app_css_end:\n"
        ".globl _binary_app_js_start\n_binary_app_js_start:\n"
        ".incbin \"main/web/app.js\"\n.byte 0\n"
        ".globl _binary_app_js_end\n_binary_app_js_end:\n"
        ".globl _binary_charset_js_start\n_binary_charset_js_start:\n"
        ".incbin \"main/web/charset.js\"\n.byte 0\n"
        ".globl _binary_charset_js_end\n_binary_charset_js_end:\n.text\n");

static kp_config_t committed;
static char response[65536], input[KP_JSON_BYTES + 1];
static size_t response_size, largest_send;
static unsigned send_calls, finished, visit_calls, save_calls;
static unsigned fail_send_at;
static bool fail_save, reading;
static const char *status, *content_type;
static esp_netif_t netif;
static httpd_uri_t routes[9];
static unsigned route_count, wifi_started, wifi_initialized;
static bool fail_http_start;

static void reset_response(void) {
    response_size = largest_send = send_calls = finished = 0;
    fail_send_at = 0; status = "200 OK"; content_type = NULL;
    response[0] = 0;
}
static bool collect(void *user, const char *data, size_t count) {
    (void)user;
    assert(response_size + count < sizeof(response));
    memcpy(response + response_size, data, count);
    response_size += count; response[response_size] = 0;
    return true;
}
esp_err_t httpd_resp_send_chunk(httpd_req_t *req, const char *data, ssize_t count) {
    (void)req;
    assert(count >= 0);
    ++send_calls;
    if (fail_send_at == send_calls || count > 512) return ESP_FAIL;
    if (!count) { assert(!data); ++finished; return ESP_OK; }
    if ((size_t)count > largest_send) largest_send = count;
    collect(NULL, data, count); return ESP_OK;
}
esp_err_t httpd_resp_send(httpd_req_t *req, const char *data, ssize_t count) {
    if (count < 0) count = strlen(data);
    return httpd_resp_send_chunk(req, data, count);
}
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *text) {
    return httpd_resp_send(req, text, -1);
}
esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *s) { (void)r; status=s; return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *s) { (void)r; content_type=s; return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *k, const char *v) {
    (void)r; assert(k && v); return ESP_OK;
}
static const char *header(httpd_req_t *r, const char *name) {
    return !strcmp(name,"Host") ? r->host : !strcmp(name,"X-Session") ? r->session : NULL;
}
size_t httpd_req_get_hdr_value_len(httpd_req_t *r, const char *name) {
    const char *s=header(r,name); return s ? strlen(s) : 0;
}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *name, char *out, size_t size) {
    const char *s=header(r,name);
    if (!s || strlen(s)>=size) return ESP_FAIL;
    strcpy(out,s); return ESP_OK;
}
int httpd_req_recv(httpd_req_t *r, char *out, size_t size) {
    if (size > r->content_len-r->received) size=r->content_len-r->received;
    // Fragmented uploads still use the bounded streaming parser.
    if (size > 13) size=13;
    memcpy(out,r->body+r->received,size); r->received+=size; return (int)size;
}
bool kp_store_read(kp_store_reader_t reader, void *user) {
    assert(!reading); reading=true; ++visit_calls;
    bool ok=reader(&committed,user); reading=false; return ok;
}
esp_err_t kp_store_save(const kp_config_t *c) {
    assert(!reading && kp_config_valid(c)); ++save_calls;
    if (fail_save) return ESP_FAIL;
    committed=*c; return ESP_OK;
}
esp_err_t httpd_start(httpd_handle_t *out, const httpd_config_t *c) {
    assert(c->max_open_sockets==3 && c->stack_size==6144 && c->lru_purge_enable);
    if (fail_http_start) return ESP_FAIL;
    *out=routes; route_count=0; return ESP_OK;
}
esp_err_t httpd_stop(httpd_handle_t h) { assert(h==routes); route_count=0; return ESP_OK; }
esp_err_t httpd_register_uri_handler(httpd_handle_t h, const httpd_uri_t *route) {
    assert(h==routes && route_count<9); routes[route_count++]=*route; return ESP_OK;
}
esp_netif_t *esp_netif_create_default_wifi_ap(void) { return &netif; }
void esp_netif_destroy_default_wifi(esp_netif_t *n) { assert(n==&netif); }
esp_err_t esp_wifi_init(const wifi_init_config_t *c) { (void)c; ++wifi_initialized; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { assert(wifi_initialized==1); --wifi_initialized; return ESP_OK; }
esp_err_t esp_wifi_start(void) { ++wifi_started; return ESP_OK; }
esp_err_t esp_wifi_stop(void) { assert(wifi_started==1); --wifi_started; return ESP_OK; }
esp_err_t esp_wifi_set_storage(int mode) { assert(mode==WIFI_STORAGE_RAM); return ESP_OK; }
esp_err_t esp_wifi_set_mode(int mode) { assert(mode==WIFI_MODE_AP); return ESP_OK; }
esp_err_t esp_wifi_set_config(int mode, const wifi_config_t *c) {
    assert(mode==WIFI_IF_AP && c->ap.max_connection==1 && c->ap.authmode==WIFI_AUTH_WPA2_PSK);
    return ESP_OK;
}
uint32_t esp_random(void) { return 1; }
esp_err_t esp_read_mac(uint8_t *out, int mode) { assert(mode==ESP_MAC_WIFI_SOFTAP); memset(out,0,6); return ESP_OK; }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 1000; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return 512; }

int main(void) {
    kp_defaults(&committed);
    assert(kp_web_start(NULL)==ESP_OK && route_count==8);
    const struct { const char *uri,*start,*end; } files[]={
        {"/",index_start,index_end},{"/app.css",css_start,css_end},
        {"/app.js",js_start,js_end},{"/charset.js",charset_start,charset_end}
    };
    for (unsigned i=0;i<sizeof(files)/sizeof(files[0]);++i) {
        reset_response();
        httpd_req_t req={.uri=files[i].uri};
        assert(static_file(&req)==ESP_OK && finished==1 && largest_send<=512);
        assert(response_size==(size_t)(files[i].end-files[i].start-1));
        assert(!memcmp(response,files[i].start,response_size));
        reset_response();
        // charset.js fits one block; fail that block rather than its terminator.
        fail_send_at=(size_t)(files[i].end-files[i].start-1)>512 ? 3 : 1;
        assert(static_file(&req)==ESP_FAIL && send_calls==fail_send_at && !finished);
    }
    httpd_req_t req={.uri="/api/config",.method=HTTP_GET,.host="192.168.4.1"};
    reset_response(); fail_alloc=true; unsigned before=allocations;
    assert(api(&req)==ESP_OK && allocations==before && finished==1 && !reading);
    assert(strstr(response,"\"version\":4") && strstr(response,"\"session\":") && largest_send<=256);
    kp_step_t steps[KP_MAX_STEPS];
    for(unsigned i=0;i<KP_MAX_STEPS;++i)
        steps[i]=(kp_step_t){{.type=KP_KEYBOARD,.keys={4}},i+1==KP_MAX_STEPS?0:5000};
    kp_config_empty(&committed); assert(kp_profile_add(&committed,"A"));
    for(unsigned i=0;i<7;++i)
        assert(kp_binding_set(&committed,0,i,i?"":"12345678901",steps,64));
    steps[29].delay_ms=0; assert(kp_binding_set(&committed,0,7,"",steps,30));
    assert(kp_config_size(&committed)==KP_CONFIG_BYTES);
    reset_response();
    assert(api(&req)==ESP_OK && allocations==before && finished==1 && largest_send<=256);
    assert(response_size>20000 && !reading);
    kp_defaults(&committed);
    reset_response(); fail_send_at=3; unsigned visits=visit_calls;
    assert(api(&req)==ESP_FAIL && visit_calls==visits+1 && !reading && !finished);
    reset_response(); assert(api(&req)==ESP_OK && !reading); // usable after a failed send
    reset_response(); req.host="example.invalid";
    assert(api(&req)==ESP_OK && !strcmp(status,"403 Forbidden"));
    req.host="192.168.4.1"; req.method=HTTP_POST; req.content_len=1;
    req.body="{"; req.session=NULL; reset_response();
    assert(api(&req)==ESP_OK && !strcmp(status,"403 Forbidden") && !req.received);
    req.session=s_session; reset_response();
    assert(api(&req)==ESP_OK && !strcmp(status,"500 Internal Server Error") && !save_calls);
    fail_alloc=false; reset_response();
    kp_config_t candidate=committed;
    assert(kp_profile_name_set(&candidate,0,"Coding"));
    assert(kp_config_encode(&candidate,collect,NULL));
    strcpy(input,response); req.body=input; req.content_len=strlen(input);
    fail_save=true; reset_response();
    assert(api(&req)==ESP_OK && !strcmp(status,"500 Internal Server Error") && save_calls==1);
    assert(!strcmp(kp_text(&committed,committed.profiles[0].name),"默认"));
    fail_save=false; req.received=0; reset_response();
    assert(api(&req)==ESP_OK && !strcmp(response,"{\"ok\":true}") && save_calls==2);
    assert(!strcmp(kp_text(&committed,committed.profiles[0].name),"Coding"));
    assert(kp_web_stop()==ESP_OK && !wifi_initialized && !wifi_started && !s_session[0]);
    fail_http_start=true;
    assert(kp_web_start(NULL)==ESP_FAIL && !wifi_initialized && !wifi_started);
    puts("Keypad HTTP bounded assets, zero-copy GET, failures and rollback: PASS");
}
