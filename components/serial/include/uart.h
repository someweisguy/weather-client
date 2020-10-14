#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t uart_init();

esp_err_t uart_deinit();

esp_err_t uart_bus_write(const void *buf, size_t size, TickType_t timeout);

esp_err_t uart_bus_read(void *buf, size_t size, TickType_t timeout);
