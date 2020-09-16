#pragma once

#include "esp_system.h"

esp_err_t uart_start();

esp_err_t uart_stop();

esp_err_t uart_write(void *buf, size_t size, time_t wait_ms);

esp_err_t uart_read(void *buf, size_t size, time_t wait_ms);
