#pragma once

#include "esp_system.h"
#include "esp_netif.h"

typedef struct
{
    esp_ip4_addr_t ip;
    int64_t up_time;
    int8_t rssi;
} wlan_data_t;

esp_err_t wlan_start();
esp_err_t wlan_stop();

esp_err_t wlan_get_data(wlan_data_t *config);