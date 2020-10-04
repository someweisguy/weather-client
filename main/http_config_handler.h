#pragma once
#include "esp_system.h"
#include "esp_http_server.h"

#include "cJSON.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#include "json_keys.h"

esp_err_t http_config_handler(httpd_req_t *r)
{
    // TODO
    
    // send response to client
    httpd_resp_set_type(r, HTTPD_TYPE_TEXT);
    httpd_resp_sendstr(r, "OK");
    
    return ESP_OK;
}