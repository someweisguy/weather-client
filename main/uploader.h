#ifndef __UPLOADER_H__
#define __UPLOADER_H__

#include "esp_system.h"
#include "esp_wifi.h"

esp_err_t uploader_get_config(wifi_config_t *wifi_config, const TickType_t timeout);

#endif // __UPLOADER_H__