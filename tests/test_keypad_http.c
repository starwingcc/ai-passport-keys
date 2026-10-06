#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake_web.h"
#include "keypad_model.h"
#include "keypad_settings.h"

static bool fail_alloc;
static unsigned allocations;
static void *web_malloc(size_t size) {
    ++allocations;
    assert(size == KP_CONFIG_BYTES || size == sizeof(kp_settings_t));
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

static kp_wifi_job_t setup_job;
static kp_wifi_config_t tested_wifi;
static bool fail_setup_start,fail_setup_stop,fail_sta_netif,fail_wifi_start,fail_wifi_stop;
static unsigned setup_active,netifs;
esp_err_t kp_wifi_setup_start(void){if(fail_setup_start)return ESP_FAIL;++setup_active;return ESP_OK;}
esp_err_t kp_wifi_setup_stop(void){if(fail_setup_stop)return ESP_FAIL;setup_active=0;return ESP_OK;}
void kp_wifi_setup_get(kp_wifi_job_t *out){*out=setup_job;}
esp_err_t kp_wifi_setup_scan(uint32_t *id){if(setup_job.busy)return ESP_ERR_INVALID_STATE;*id=++setup_job.id;setup_job.kind=KP_WIFI_SCAN;setup_job.busy=true;return ESP_OK;}
esp_err_t kp_wifi_setup_test(const kp_wifi_config_t *candidate,uint32_t *id){if(setup_job.busy)return ESP_ERR_INVALID_STATE;tested_wifi=*candidate;*id=++setup_job.id;setup_job.kind=KP_WIFI_TEST;setup_job.busy=true;return ESP_OK;}
static kp_wifi_config_t committed_wifi;
void kp_store_wifi_get(kp_wifi_config_t *out){*out=committed_wifi;}
esp_err_t kp_store_wifi_save(const kp_wifi_config_t *wifi){if(fail_alloc)return ESP_FAIL;committed_wifi=*wifi;return ESP_OK;}
kp_network_status_t kp_network_status(void){return KP_NET_PAUSED;}
uint64_t kp_store_time_read(uint64_t now){(void)now;return 0;}
static kp_config_t committed;
static kp_home_config_t committed_home; static kp_clock_t test_clock;
int64_t esp_timer_get_time(void){return 1000000;}
void kp_store_home_get(kp_home_config_t *out){*out=committed_home;}
esp_err_t kp_store_home_save(const kp_home_config_t *home){if(fail_alloc)return ESP_FAIL;committed_home=*home;return ESP_OK;}
bool kp_store_time_sync(uint64_t epoch,uint64_t now){return kp_clock_sync(&test_clock,epoch,now);}
static char response[65536], input[KP_SETTINGS_JSON_BYTES + 1];
static size_t response_size, largest_send;
static unsigned send_calls, finished, visit_calls, save_calls;
static unsigned fail_send_at;
static bool fail_save, reading;
static const char *status, *content_type;
static esp_netif_t netif, station_netif;
static httpd_uri_t routes[18];
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
bool kp_store_settings_read(kp_store_settings_reader_t reader, void *user) {
    assert(!reading); reading=true; ++visit_calls;
    kp_settings_t settings={.version=1,.home=committed_home,.wifi=committed_wifi,.config=committed};
    bool ok=reader(&settings,user); reading=false; return ok;
}
esp_err_t kp_store_settings_save(const kp_settings_t *settings) {
    assert(!reading && kp_settings_valid(settings,false)); ++save_calls;
    if (fail_save) return ESP_FAIL;
    committed=settings->config;committed_home=settings->home;committed_wifi=settings->wifi;
    return ESP_OK;
}
esp_err_t httpd_start(httpd_handle_t *out, const httpd_config_t *c) {
    assert(c->max_uri_handlers==18 && c->max_open_sockets==3 && c->stack_size==6144 && c->lru_purge_enable);
    if (fail_http_start) return ESP_FAIL;
    *out=routes; route_count=0; return ESP_OK;
}
esp_err_t httpd_stop(httpd_handle_t h) { assert(h==routes); route_count=0; return ESP_OK; }
esp_err_t httpd_register_uri_handler(httpd_handle_t h, const httpd_uri_t *route) {
    assert(h==routes && route_count<18); routes[route_count++]=*route; return ESP_OK;
}
esp_netif_t *esp_netif_create_default_wifi_ap(void) { ++netifs;return &netif; }
esp_netif_t *esp_netif_create_default_wifi_sta(void) { if(fail_sta_netif)return NULL;++netifs;return &station_netif; }
void esp_netif_destroy_default_wifi(esp_netif_t *n) { assert((n==&netif||n==&station_netif)&&netifs);--netifs; }
esp_err_t esp_wifi_init(const wifi_init_config_t *c) { (void)c; ++wifi_initialized; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { assert(wifi_initialized==1); --wifi_initialized; return ESP_OK; }
esp_err_t esp_wifi_start(void) { if(fail_wifi_start)return ESP_FAIL;++wifi_started; return ESP_OK; }
esp_err_t esp_wifi_stop(void) { assert(wifi_started==1);if(fail_wifi_stop)return ESP_FAIL; --wifi_started; return ESP_OK; }
esp_err_t esp_wifi_set_storage(int mode) { assert(mode==WIFI_STORAGE_RAM); return ESP_OK; }
esp_err_t esp_wifi_set_mode(int mode) { assert(mode==WIFI_MODE_APSTA); return ESP_OK; }
esp_err_t esp_wifi_set_config(int mode, const wifi_config_t *c) {
    if(mode==WIFI_IF_AP)assert(c->ap.max_connection==1 && c->ap.authmode==WIFI_AUTH_WPA2_PSK);
    else assert(mode==WIFI_IF_STA&&!c->sta.ssid[0]&&!c->sta.password[0]);
    return ESP_OK;
}
uint32_t esp_random(void) { return 1; }
esp_err_t esp_read_mac(uint8_t *out, int mode) { assert(mode==ESP_MAC_WIFI_SOFTAP); memset(out,0,6); return ESP_OK; }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 1000; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return 512; }

int main(void) {
    kp_wifi_defaults(&committed_wifi);
    kp_home_defaults(&committed_home);
    kp_defaults(&committed);
    assert(kp_web_start(NULL)==ESP_OK && route_count==18);
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
    assert(strstr(response,"\"version\":5") && strstr(response,"\"session\":") && largest_send<=256);
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
    // New routes use the same Host/session checks and bounded transport.
    kp_home_config_t home=committed_home;strcpy(home.name,"星翼");home.avatar=2;
    reset_response();assert(kp_home_encode(&home,collect,NULL));strcpy(input,response);
    req=(httpd_req_t){.uri="/api/home",.method=HTTP_POST,.host="192.168.4.1",.body=input,.content_len=strlen(input)};
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"403 Forbidden")&&!req.received);
    req.session=s_session;fail_alloc=true;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"500 Internal Server Error"));assert(!strcmp(committed_home.name,"USER"));
    fail_alloc=false;req.received=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&!strcmp(committed_home.name,"星翼"));
    req.method=HTTP_GET;reset_response();assert(api(&req)==ESP_OK&&finished==1&&strstr(response,"星翼"));
    req=(httpd_req_t){.uri="/api/time",.method=HTTP_POST,.host="192.168.4.1",.session=s_session,.body="{\"epoch\":1791167040}",.content_len=20};
    req.content_len=strlen(req.body);reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&kp_clock_read(&test_clock,2000)==1791167041);
    req.body="{\"epoch\":0}";req.content_len=strlen(req.body);req.received=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"400 Bad Request")&&test_clock.epoch==1791167040);
    req=(httpd_req_t){.uri="/api/wifi",.method=HTTP_GET,.host="192.168.4.1"};
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"403 Forbidden"));
    req.session=s_session;reset_response();assert(api(&req)==ESP_OK&&finished==1&&strstr(response,"has_password"));
    req.method=HTTP_POST;req.body="{\"ssid\":\"test-network\",\"password\":\"test-pass\",\"keep_password\":false}";req.content_len=strlen(req.body);
    fail_alloc=true;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"500 Internal Server Error")&&!committed_wifi.ssid[0]);
    fail_alloc=false;req.received=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&!strcmp(committed_wifi.password,"test-pass"));
    req.method=HTTP_GET;reset_response();assert(api(&req)==ESP_OK&&finished==1&&strstr(response,"test-network")&&!strstr(response,"test-pass"));
    req.method=HTTP_POST;req.body="{\"ssid\":\"test-network\",\"password\":\"\",\"keep_password\":true}";req.content_len=strlen(req.body);req.received=0;
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&!strcmp(committed_wifi.password,"test-pass"));
    req.body="{\"ssid\":\"other-network\",\"password\":\"\",\"keep_password\":true}";req.content_len=strlen(req.body);req.received=0;
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"400 Bad Request")&&!strcmp(committed_wifi.ssid,"test-network"));
    req.body="{\"ssid\":\"\",\"password\":\"\",\"keep_password\":false}";req.content_len=strlen(req.body);req.received=0;
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&!committed_wifi.password[0]);
    // Scan/test are authenticated queued jobs. Neither test nor its GET exposes
    // credentials or commits a candidate to persistent Wi-Fi configuration.
    req=(httpd_req_t){.uri="/api/wifi/scan",.method=HTTP_POST,.host="192.168.4.1"};
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"403 Forbidden")&&!setup_job.id);
    req.session=s_session;req.body="{}";req.content_len=2;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"400 Bad Request"));
    req.content_len=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&setup_job.busy&&strstr(response,"id"));
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"409 Conflict"));setup_job.busy=false;
    req=(httpd_req_t){.uri="/api/wifi/test",.method=HTTP_POST,.host="192.168.4.1",.session=s_session,.body="{\"ssid\":\"candidate-network\",\"password\":\"test-pass\",\"keep_password\":false}"};req.content_len=strlen(req.body);
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&!strcmp(tested_wifi.password,"test-pass")&&!committed_wifi.ssid[0]);
    setup_job.state=KP_WIFI_AUTH_FAILED;setup_job.busy=false;strcpy(setup_job.ssid,tested_wifi.ssid);
    kp_wifi_scan_add(&setup_job,"nearby",-40,true);
    req.uri="/api/wifi/job";req.method=HTTP_GET;req.session=NULL;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"403 Forbidden"));
    req.session=s_session;reset_response();assert(api(&req)==ESP_OK&&finished==1&&strstr(response,"candidate-network")&&strstr(response,"nearby")&&!strstr(response,"test-pass")&&largest_send<=256);
    req.uri="/api/wifi/test";req.method=HTTP_POST;req.body="{\"ssid\":\"\",\"password\":\"\",\"keep_password\":false}";req.content_len=strlen(req.body);req.received=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"400 Bad Request"));
    // Saved-password testing passes the password internally, without returning it.
    strcpy(committed_wifi.ssid,"saved-network");strcpy(committed_wifi.password,"saved-test-pass");
    req.body="{\"ssid\":\"saved-network\",\"password\":\"\",\"keep_password\":true}";req.content_len=strlen(req.body);req.received=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK")&&!strcmp(tested_wifi.password,"saved-test-pass"));
    // Complete reads expose credentials only to the authorized setup session.
    req=(httpd_req_t){.uri="/api/settings",.method=HTTP_GET,.host="192.168.4.1"};
    reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"403 Forbidden")&&!strstr(response,"saved-test-pass"));
    req.session=s_session;unsigned allocations_before=allocations;
    reset_response();assert(api(&req)==ESP_OK&&finished==1&&strstr(response,"saved-test-pass")&&strstr(response,"home")&&strstr(response,"config")&&allocations==allocations_before&&largest_send<=256);
    kp_settings_t all_before={.version=1,.home=committed_home,.wifi=committed_wifi,.config=committed},all_next=all_before;
    strcpy(all_next.home.name,"Complete");strcpy(all_next.wifi.ssid,"complete-network");strcpy(all_next.wifi.password,"complete-pass");
    assert(kp_profile_add(&all_next.config,"Complete"));
    reset_response();assert(kp_settings_encode(&all_next,collect,NULL));strcpy(input,response);
    assert(kp_settings_decode_string(input,&all_next)); // Compare the canonical wire representation.
    req.method=HTTP_POST;req.body=input;req.content_len=strlen(input);req.received=0;
    fail_alloc=true;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"500 Internal Server Error"));
    fail_alloc=false;fail_save=true;req.received=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"500 Internal Server Error"));
    assert(!memcmp(&committed,&all_before.config,sizeof(committed))&&!memcmp(&committed_home,&all_before.home,sizeof(committed_home))&&!memcmp(&committed_wifi,&all_before.wifi,sizeof(committed_wifi)));
    fail_save=false;req.received=0;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"200 OK"));
    assert(!memcmp(&committed,&all_next.config,sizeof(committed))&&!memcmp(&committed_home,&all_next.home,sizeof(committed_home))&&!memcmp(&committed_wifi,&all_next.wifi,sizeof(committed_wifi)));
    req.body="{\"version\":1}";req.content_len=strlen(req.body);req.received=0;
    unsigned saves_before=save_calls;reset_response();assert(api(&req)==ESP_OK&&!strcmp(status,"400 Bad Request")&&save_calls==saves_before);
    assert(kp_web_stop()==ESP_OK && !wifi_initialized && !wifi_started && !s_session[0]);
    assert(!netifs&&!setup_active);
    for(unsigned fault=0;fault<4;++fault){
        fail_http_start=fault==0;fail_sta_netif=fault==1;fail_setup_start=fault==2;fail_wifi_start=fault==3;
        assert(kp_web_start(NULL)!=ESP_OK&&!wifi_initialized&&!wifi_started&&!netifs&&!setup_active);
    }
    fail_http_start=fail_sta_netif=fail_setup_start=fail_wifi_start=false;
    assert(kp_web_start(NULL)==ESP_OK);fail_setup_stop=true;
    assert(kp_web_stop()==ESP_FAIL&&wifi_started&&wifi_initialized&&netifs==2);fail_setup_stop=false;
    fail_wifi_stop=true;assert(kp_web_stop()==ESP_FAIL&&wifi_started&&wifi_initialized&&netifs==2);fail_wifi_stop=false;
    assert(kp_web_stop()==ESP_OK&&!wifi_started&&!wifi_initialized&&!netifs&&!setup_active);
    puts("Keypad HTTP bounded assets, zero-copy GET, failures and rollback: PASS");
}
