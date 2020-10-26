#pragma once

#include "esp_system.h"
#include "mqtt.h"

#define HASS_SENSOR_TOPIC "homeassistant/sensor" 
#define STATE_TOPIC_SUFFIX "/state"


esp_err_t mqtt_request_handler(mqtt_req_t *r);

esp_err_t mqtt_homeassistant_handler(mqtt_req_t *r);

esp_err_t mqtt_config_handler(mqtt_req_t *r);
esp_err_t mqtt_data_handler(mqtt_req_t *r);
