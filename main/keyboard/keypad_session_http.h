#pragma once

#include "esp_http_server.h"
#include "keypad_session.h"

// HTTP-task handlers shared by both LAN services. POSTs require the device
// address and same origin; they only admit pages to the manually enabled mode.
esp_err_t kp_session_http(httpd_req_t *req, const char *ip);
