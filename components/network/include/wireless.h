#pragma once

#include "cJSON.h"
#include "esp_system.h"

typedef enum 
{
    MQTT_SENSOR,
    MQTT_BINARY_SENSOR,
    MQTT_MAX
} discovery_type_t;

typedef struct
{
    discovery_type_t type;
    char *availability_topic;
    struct
    {
        char *identifiers;
        char *manufacturer;
        char *model;
        char *name;
        char *sw_version;
    } device;
    char *device_class;
    uint32_t expire_after;
    bool force_update;
    char *json_attributes_template;
    char *json_attributes_topic;
    char *name;
    char *payload_available;
    char *payload_not_available;
    uint8_t qos;
    char *state_topic;
    char *unique_id;
    char *value_template;
    union
    {
        struct
        {
            char *icon;
        } sensor;
        struct
        {
            uint32_t off_delay;
            char *payload_on;
            char *payload_off;
        } binary_sensor;
    };

} mqtt_discovery_t;

esp_err_t wireless_start(const char *mqtt_broker);

esp_err_t mqtt_publish(const char *topic, const char *message, int qos, bool retain);

esp_err_t mqtt_publish_json(const char *topic, cJSON *json, int qos, bool retain);

double wireless_get_elevation();

int8_t wireless_get_rssi();

esp_err_t mqtt_publish_discovery(const char *topic, mqtt_discovery_t discovery);