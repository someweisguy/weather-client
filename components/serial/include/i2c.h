#pragma once

#include "esp_system.h"

esp_err_t i2c_start();

esp_err_t i2c_stop();

esp_err_t i2c_read(char addr, char reg, void *buf, size_t size, time_t wait_ms);

esp_err_t i2c_write(char addr, char reg, const void *buf, size_t size, time_t wait_ms);
