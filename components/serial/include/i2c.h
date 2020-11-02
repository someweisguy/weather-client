#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t i2c_init();

esp_err_t i2c_deinit();

esp_err_t i2c_bus_read(char addr, char reg, void *buf, size_t size,
                       TickType_t timeout);

esp_err_t i2c_bus_write(char addr, char reg, const void *buf, size_t size,
                        TickType_t timeout);

esp_err_t i2c_bus_write_no_ack(char addr, char reg, const void *buf,
                               size_t size, TickType_t timeout);