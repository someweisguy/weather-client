#pragma once

#include "esp_system.h"
#include "esp_http_server.h"

esp_err_t http_data_handler(httpd_req_t *r);

esp_err_t http_restart_handler(httpd_req_t *r);