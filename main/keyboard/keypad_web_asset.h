#pragma once
#include "esp_http_server.h"
// Flash-backed assets have a known Content-Length; no whole-asset RAM copy.
esp_err_t kp_web_asset(httpd_req_t *req, const char *start, const char *end, const char *mime);
