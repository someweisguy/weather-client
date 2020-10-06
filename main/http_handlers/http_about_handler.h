#pragma once
#include "esp_system.h"
#include "esp_http_server.h"

#include "cJSON.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#include "json_keys.h"

char *about_handler()
{
    esp_err_t err;
    cJSON *root = cJSON_CreateObject();

    // add the compilation timestamp
    cJSON_AddStringToObject(root, "compiled", __DATE__ " " __TIME__);

    // get the system up time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    const uint64_t system_up_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    cJSON_AddNumberToObject(root, "up_time", system_up_time);

    // create a root for the sensors
    cJSON *sensor_root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "sensors", sensor_root);

    // report max17043 status
    max17043_config_t max_config;
    err = max17043_get_config(&max_config);
    cJSON_AddStringToObject(sensor_root, "max17043", esp_err_to_name(err));

    // report bme280 status
    bme280_config_t bme_config;
    err = bme280_get_config(&bme_config);
    cJSON_AddStringToObject(sensor_root, "bme280", esp_err_to_name(err));

    // reposrt pms5003 status
    pms5003_config_t pms_config;
    err = pms5003_get_config(&pms_config);
    cJSON_AddStringToObject(sensor_root, "pms5003", esp_err_to_name(err));

    // report sph0645 status
    sph0645_config_t sph_config;
    err = sph0645_get_config(&sph_config);
    cJSON_AddStringToObject(sensor_root, "sph0645", esp_err_to_name(err));

    // render the json as a string
    char *response = cJSON_Print(root);

    cJSON_Delete(root);

    return response;
}

esp_err_t http_about_handler(httpd_req_t *r)
{
    // send json back to the client
    httpd_resp_set_type(r, HTTPD_TYPE_JSON);

    // generate the response to the client
    char *response = about_handler();

    // send the response to the client
    httpd_resp_set_status(r, HTTPD_200);
    httpd_resp_sendstr(r, response);

    // free the response
    free(response);

    return ESP_OK;
}