#pragma once

#include "esp_system.h"
#include "esp_sntp.h"

esp_err_t wireless_start(const char *mqtt_broker, sntp_sync_time_cb_t callback);