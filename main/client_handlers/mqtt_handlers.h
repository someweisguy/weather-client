#pragma once

#include "esp_system.h"
#include "mqtt.h"

esp_err_t mqtt_about_handler(mqtt_req_t *r);
esp_err_t mqtt_config_handler(mqtt_req_t *r);
esp_err_t mqtt_data_handler(mqtt_req_t *r);