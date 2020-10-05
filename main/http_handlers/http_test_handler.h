#pragma once
#include "esp_system.h"
#include "esp_http_server.h"

#include "cJSON.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#include "json_keys.h"

esp_err_t http_test_handler(httpd_req_t *r)
{
    esp_err_t err;

    // send json back to the client
    httpd_resp_set_type(r, HTTPD_TYPE_JSON);

    cJSON* root = cJSON_CreateObject();

    max17043_config_t max_config;
    err = max17043_get_config(&max_config);
    cJSON_AddStringToObject(root, "MAX17043", esp_err_to_name(err));

    bme280_config_t bme_config;
    err = bme280_get_config(&bme_config);
    cJSON_AddStringToObject(root, "BME280", esp_err_to_name(err));

    pms5003_config_t pms_config;
    err = pms5003_get_config(&pms_config);
    cJSON_AddStringToObject(root, "PMS5003", esp_err_to_name(err));

    sph0645_config_t sph_config;
    err = sph0645_get_config(&sph_config);
    cJSON_AddStringToObject(root, "SPH0645", esp_err_to_name(err));

    // render the json as a string
    char *rendered = cJSON_Print(root);

    // send the response to the client
    httpd_resp_set_status(r, HTTPD_200);
    httpd_resp_sendstr(r, rendered);

    // free resources
    cJSON_Delete(root);
    free(rendered);

    return ESP_OK;
}