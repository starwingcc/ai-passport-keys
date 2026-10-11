#pragma once

#include "esp_http_server.h"
#include "keypad_session.h"

// HTTP-task handlers shared by both LAN services. POSTs require the device
// address and same origin; only an explicit open can switch modes.
esp_err_t kp_session_http(httpd_req_t *req, const char *ip);
// Loading a document consumes no lease; its later join must still be valid.
bool kp_session_document(httpd_req_t *req, kp_session_mode_t mode);
esp_err_t kp_session_bootstrap(httpd_req_t *req);
