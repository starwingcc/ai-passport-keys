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
    int method;
    size_t content_len, received;
} httpd_req_t;
typedef struct {
    const char *uri;
    int method;
    esp_err_t (*handler)(httpd_req_t *);
} httpd_uri_t;
typedef struct {
    unsigned max_open_sockets, max_uri_handlers, stack_size;
    unsigned recv_wait_timeout, send_wait_timeout;
    bool lru_purge_enable;
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
esp_netif_t *esp_netif_create_default_wifi_ap(void);
void esp_netif_destroy_default_wifi(esp_netif_t *);
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct {
    struct {
        uint8_t ssid[32], password[64];
        unsigned ssid_len, channel, max_connection, authmode;
    } ap;
} wifi_config_t;
enum { WIFI_STORAGE_RAM, WIFI_MODE_AP, WIFI_IF_AP, WIFI_AUTH_WPA2_PSK };
esp_err_t esp_wifi_init(const wifi_init_config_t *);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_set_storage(int);
esp_err_t esp_wifi_set_mode(int);
esp_err_t esp_wifi_set_config(int, const wifi_config_t *);
uint32_t esp_random(void);
#define ESP_MAC_WIFI_SOFTAP 1
esp_err_t esp_read_mac(uint8_t *, int);
#define MALLOC_CAP_8BIT 1
size_t heap_caps_get_free_size(unsigned);
size_t heap_caps_get_largest_free_block(unsigned);
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
