#pragma once

#include "esp_system.h"

esp_err_t wifi_start();
esp_err_t wifi_stop();

int8_t wifi_get_rssi();