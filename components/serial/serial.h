#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t serial_start();

esp_err_t serial_i2c_read(char addr, char reg, void *buf, size_t size, 
    TickType_t timeout);

esp_err_t serial_i2c_write(char addr, char reg, const void *buf, size_t size, 
    bool ack_en, TickType_t timeout);

esp_err_t serial_uart_read(void *buf, size_t size, TickType_t timeout);

esp_err_t serial_uart_write(const void *src, size_t size, TickType_t timeout);

esp_err_t serial_uart_flush();

esp_err_t serial_i2s_read(void *buf, size_t size, TickType_t timeout);

#ifdef __cplusplus
}
#endif
