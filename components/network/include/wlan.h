#pragma once

#include "esp_system.h"
#include "esp_netif.h"

typedef struct
{
    esp_ip4_addr_t ip; // The IP address in use by the device.
    char ip_str[16];   // The IP address in use by the device as a string.
    int64_t up_time;   // WiFi uptime (ms).
    int8_t rssi;       // Received Signal Strength Indicator (dBm). See https://www.metageek.com/training/resources/understanding-rssi.html for more info.
} wlan_data_t;

esp_err_t wlan_start();
esp_err_t wlan_stop();

esp_err_t wlan_get_data(wlan_data_t *data);