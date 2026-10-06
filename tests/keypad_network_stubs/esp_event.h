#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef const char *esp_event_base_t;
typedef void *esp_event_handler_instance_t;
typedef void (*network_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
extern esp_event_base_t WIFI_EVENT, IP_EVENT;
enum { ESP_EVENT_ANY_ID = -1, WIFI_EVENT_STA_START = 1, WIFI_EVENT_STA_DISCONNECTED, IP_EVENT_STA_GOT_IP, WIFI_EVENT_SCAN_DONE, IP_EVENT_STA_LOST_IP };
typedef struct { unsigned status; } wifi_event_sta_scan_done_t;
typedef struct { uint16_t reason; } wifi_event_sta_disconnected_t;
esp_err_t esp_event_handler_instance_register(esp_event_base_t, int32_t, network_event_handler_t, void *, esp_event_handler_instance_t *);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t, int32_t, esp_event_handler_instance_t);

typedef struct { void *esp_netif; } ip_event_got_ip_t;
