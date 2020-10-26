#pragma once

#include "esp_system.h"
#include "mqtt.h"

#ifdef CONFIG_OUTSIDE_STATION
#define MQTT_CLIENT_NAME "outside"
#define MQTT_CLIENT_NAME_UPPER "Outside"
#define MQTT_DEVICE_NAME "Outdoor Weather Station"
#define MQTT_MODEL_NAME "Outdoor Model"
#elif defined(CONFIG_INSIDE_STATION)
#define MQTT_CLIENT_NAME "inside"
#define MQTT_CLIENT_NAME_UPPER "Inside"
#define MQTT_DEVICE_NAME "Indoor Weather Station"
#define MQTT_MODEL_NAME "Indoor Model"
#elif defined(CONFIG_WIND_AND_RAIN_STATION)
#define MQTT_CLIENT_NAME "wind_rain"
#define MQTT_DEVICE_NAME "Wind & Rain Station"
#define MQTT_MODEL_NAME "Wind & Rain Model"
#endif

#define MQTT_TOPIC_BASE         "weather-station"
#define MQTT_BASE_TOPIC         MQTT_TOPIC_BASE "/" MQTT_CLIENT_NAME

#define MQTT_AVAILABLE_TOPIC    MQTT_BASE_TOPIC "/available"
#define MQTT_STATE_TOPIC        MQTT_BASE_TOPIC "/state"


#define HASS_DEVICE_NAME "gndctrl-" MQTT_CLIENT_NAME


#define HASS_SENSOR_CONFIG_PREFIX "homeassistant/sensor"
#define HASS_CONFIG_SUFFIX "/config"

#define HASS_TOPIC(n) (HASS_SENSOR_CONFIG_PREFIX "/" n "/" MQTT_CLIENT_NAME "/config")








#define HASS_SENSOR_TOPIC "homeassistant/sensor" 




esp_err_t mqtt_request_handler(mqtt_req_t *r);

esp_err_t mqtt_homeassistant_handler(mqtt_req_t *r);

esp_err_t mqtt_config_handler(mqtt_req_t *r);
esp_err_t mqtt_data_handler(mqtt_req_t *r);
