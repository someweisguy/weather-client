#pragma once

#include "esp_system.h"
#include "driver/i2s.h"

esp_err_t i2s_init(void);

esp_err_t i2s_end(void);
