#pragma once

#include "esp_system.h"
#include "esp_wifi.h"

esp_err_t uploader_get_config(wifi_config_t *wifi_config, const TickType_t timeout);
