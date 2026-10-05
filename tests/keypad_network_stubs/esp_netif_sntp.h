#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <sys/time.h>
typedef struct {
    bool start, wait_for_sync;
    unsigned num_of_servers;
    const char *servers[3];
    void (*sync_cb)(struct timeval *);
} esp_sntp_config_t;
#define ESP_SNTP_SERVER_LIST(...) { __VA_ARGS__ }
#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(count, list) { .start=true, .wait_for_sync=true, .num_of_servers=count, .servers=list }
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *);
void esp_netif_sntp_deinit(void);
