#pragma once

#include "esp_system.h"

esp_err_t max17043_start();

float max17043_get_battery_millivolts();

float max17043_get_battery_percentage();