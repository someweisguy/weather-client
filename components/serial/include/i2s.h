#pragma once

#include "esp_system.h"

esp_err_t i2s_init(void);

esp_err_t i2s_end(void);

esp_err_t i2s_master_read(void *buf, size_t size, time_t wait_ms);