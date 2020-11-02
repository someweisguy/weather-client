#pragma once

#include "esp_system.h"
#include "cJSON.h"

esp_err_t wireless_start(const char *mqtt_broker);

esp_err_t mqtt_publish(const char *topic, const char *message, int qos, bool retain);

esp_err_t mqtt_publish_json(const char *topic, cJSON *json, int qos, bool retain);

double wireless_get_elevation();

int8_t wireless_get_rssi();