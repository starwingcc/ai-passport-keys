#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "esp_err.h"

typedef void *httpd_handle_t;
enum { HTTP_GET, HTTP_POST };
typedef struct {
    const char *uri, *host, *session, *body;
    int method, fd;
    const char *origin;
    size_t content_len, received;
} httpd_req_t;
typedef struct {
    const char *uri;
    int method;
    esp_err_t (*handler)(httpd_req_t *);
    bool is_websocket;
    esp_err_t (*ws_pre_handshake_cb)(httpd_req_t *);
} httpd_uri_t;
typedef struct {
    unsigned max_open_sockets, max_uri_handlers, stack_size;
    unsigned recv_wait_timeout, send_wait_timeout;
    unsigned backlog_conn;
    bool lru_purge_enable;
    esp_err_t (*open_fn)(httpd_handle_t,int);
    void (*close_fn)(httpd_handle_t,int);
} httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
esp_err_t httpd_resp_set_status(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
esp_err_t httpd_resp_send(httpd_req_t *, const char *, ssize_t);
esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *);
esp_err_t httpd_resp_send_chunk(httpd_req_t *, const char *, ssize_t);
size_t httpd_req_get_hdr_value_len(httpd_req_t *, const char *);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *, const char *, char *, size_t);
int httpd_req_recv(httpd_req_t *, char *, size_t);
esp_err_t httpd_start(httpd_handle_t *, const httpd_config_t *);
esp_err_t httpd_stop(httpd_handle_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);

typedef struct { int unused; } esp_netif_t;
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip, netmask, gw; } esp_netif_ip_info_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(p) (unsigned)((p)->addr&255),(unsigned)(((p)->addr>>8)&255),(unsigned)(((p)->addr>>16)&255),(unsigned)((p)->addr>>24)
esp_err_t esp_netif_get_ip_info(esp_netif_t *, esp_netif_ip_info_t *);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
void esp_netif_destroy_default_wifi(esp_netif_t *);
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct {
    struct {
        uint8_t ssid[32], password[64];
        unsigned ssid_len, channel, max_connection, authmode;
    } ap;
    struct {
        uint8_t ssid[32], password[64];
        struct { unsigned authmode; } threshold;
    } sta;
} wifi_config_t;
enum { WIFI_STORAGE_RAM, WIFI_MODE_AP, WIFI_IF_AP, WIFI_AUTH_WPA2_PSK, WIFI_MODE_STA, WIFI_IF_STA, WIFI_AUTH_OPEN, WIFI_MODE_APSTA };
typedef struct { uint8_t ssid[33]; int8_t rssi; unsigned authmode; } wifi_ap_record_t;
#define ESP_ERR_WIFI_NOT_CONNECT 4
esp_err_t esp_wifi_init(const wifi_init_config_t *);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_set_storage(int);
esp_err_t esp_wifi_set_mode(int);
esp_err_t esp_wifi_set_config(int, const wifi_config_t *);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_scan_start(const void *, bool);
esp_err_t esp_wifi_scan_stop(void);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *);
esp_err_t esp_wifi_scan_get_ap_record(wifi_ap_record_t *);
esp_err_t esp_wifi_clear_ap_list(void);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *);
uint32_t esp_random(void);
#define ESP_MAC_WIFI_SOFTAP 1
esp_err_t esp_read_mac(uint8_t *, int);
#define MALLOC_CAP_8BIT 1
#define MALLOC_CAP_DMA 8
#define MALLOC_CAP_RTCRAM 32
size_t heap_caps_get_free_size(unsigned);
size_t heap_caps_get_largest_free_block(unsigned);
size_t heap_caps_get_minimum_free_size(unsigned);
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)

typedef struct { bool final,fragmented;int type;uint8_t *payload;size_t len; } httpd_ws_frame_t;
enum {HTTPD_WS_TYPE_TEXT=1,HTTPD_WS_TYPE_BINARY=2};
int httpd_req_to_sockfd(httpd_req_t *);
#define HTTPD_SOCK_ERR_FAIL -1
typedef int (*httpd_send_func_t)(httpd_handle_t, int, const char *, size_t, int);
esp_err_t httpd_sess_set_send_override(httpd_handle_t, int, httpd_send_func_t);
esp_err_t httpd_ws_recv_frame(httpd_req_t *,httpd_ws_frame_t *,size_t);
esp_err_t httpd_ws_send_frame_async(httpd_handle_t,int,httpd_ws_frame_t *);
enum { HTTPD_WS_CLIENT_INVALID, HTTPD_WS_CLIENT_HTTP, HTTPD_WS_CLIENT_WEBSOCKET };
int httpd_ws_get_fd_info(httpd_handle_t,int);
esp_err_t httpd_sess_trigger_close(httpd_handle_t,int);
esp_err_t httpd_queue_work(httpd_handle_t,void (*)(void *),void *);
