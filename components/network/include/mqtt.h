#pragma once

#include "esp_system.h"

esp_err_t mqtt_start(const char* mqtt_broker);

esp_err_t mqtt_stop();