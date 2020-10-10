#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t i2c_start();

esp_err_t i2c_stop();

esp_err_t i2c_bus_read(char addr, char reg, void *buf, size_t size, TickType_t timeout);

esp_err_t i2c_bus_write(char addr, char reg, const void *buf, size_t size, TickType_t timeout);

esp_err_t i2c_bus_write_no_ack(char addr, char reg, const void *buf, size_t size, TickType_t timeout);