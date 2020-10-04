#pragma once
#include "esp_system.h"
#include "esp_http_server.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#include "json_keys.h"

esp_err_t http_put_handler(httpd_req_t *r)
{
    // set http response to json
    httpd_resp_set_type(r, HTTPD_TYPE_JSON);

    httpd_resp_sendstr(r, "{\n   \"data\": \"Hello world!\"\n}");
    
    return ESP_OK;
}