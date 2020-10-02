#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t i2s_init(void);

esp_err_t i2s_deinit(void);

esp_err_t i2s_bus_read(void *buf, size_t size, TickType_t timeout);