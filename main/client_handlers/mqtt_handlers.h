#pragma once

#include "esp_system.h"
#include "mqtt.h"

#ifdef CONFIG_OUTSIDE_STATION
#define MQTT_CLIENT_NAME "outside"
#elif defined(CONFIG_INSIDE_STATION)
#define MQTT_CLIENT_NAME "inside"
#elif defined(CONFIG_WIND_AND_RAIN_STATION)
#define MQTT_CLIENT_NAME "wind_rain"
#endif

#define MQTT_TOPIC_BASE         "weather-station"
#define MQTT_BASE_TOPIC         MQTT_TOPIC_BASE "/" MQTT_CLIENT_NAME

#define MQTT_STATE_TOPIC        MQTT_TOPIC_BASE "/" MQTT_CLIENT_NAME "/state"
#define MQTT_AVAILABLE_TOPIC(n) MQTT_TOPIC_BASE "/" MQTT_CLIENT_NAME "/" n "/available"


esp_err_t mqtt_request_handler(mqtt_req_t *r);

esp_err_t mqtt_homeassistant_handler(mqtt_req_t *r);

esp_err_t mqtt_config_handler(mqtt_req_t *r);
esp_err_t mqtt_data_handler(mqtt_req_t *r);
