#pragma once

#include "esp_system.h"
#include "mqtt.h"

#ifdef CONFIG_OUTSIDE_STATION
#define MQTT_CLIENT_NAME "outside"
#elif defined(CONFIG_INSIDE_STATION)
#define MQTT_CLIENT_NAME "inside"
#elif defined(CONFIG_WIND_AND_RAIN_STATION)
#define MQTT_CLIENT_NAME "wind&rain"
#endif

#define TOPIC_PREFIX "weather-station/" MQTT_CLIENT_NAME "/"

#define AVAILABLE_TOPIC TOPIC_PREFIX "available"
#define STATUS_TOPIC    TOPIC_PREFIX "status"
#define ELEVATION_TOPIC TOPIC_PREFIX "elevation"
#define SYSTEM_TOPIC    TOPIC_PREFIX "system"
#define CLIMATE_TOPIC   TOPIC_PREFIX "climate"
#define SMOKE_TOPIC     TOPIC_PREFIX "smoke"
#define NOISE_TOPIC     TOPIC_PREFIX "noise"


esp_err_t mqtt_request_handler(mqtt_req_t *r);

esp_err_t mqtt_homeassistant_handler(mqtt_req_t *r);

esp_err_t mqtt_config_handler(mqtt_req_t *r);
esp_err_t mqtt_data_handler(mqtt_req_t *r);
