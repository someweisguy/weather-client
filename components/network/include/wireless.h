#pragma once

#include "esp_system.h"

esp_err_t wireless_start(const char *mqtt_broker);

esp_err_t mqtt_publish(const char *topic, const char *message, int qos, bool retain);

double wireless_get_elevation();

int8_t wireless_get_rssi();