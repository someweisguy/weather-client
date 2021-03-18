#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t wireless_start(const char *ssid, const char *password,
    const char *broker, TickType_t timeout);

esp_err_t wireless_stop(TickType_t timeout);

esp_err_t wireless_synchronize_time(const char *server, TickType_t timeout);

esp_err_t wireless_get_location(double *latitude, double *longitude, 
    double *elevation);

esp_err_t wireless_get_data(cJSON *json);

esp_err_t wireless_publish(const char *topic, cJSON* json, int qos, 
    bool retain, TickType_t timeout);

#ifdef __cplusplus
}
#endif