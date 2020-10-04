#pragma once

#include "esp_system.h"
#include "esp_http_server.h"

esp_err_t http_start();

esp_err_t http_stop();

esp_err_t http_register_handler(const char *uri, const httpd_method_t method, esp_err_t (*handler)(httpd_req_t *), void* user_ctx);