#pragma once

#include "esp_system.h"
#include "mqtt_client.h"

typedef struct
{
    esp_mqtt_client_handle_t *client;
    size_t content_len;
    const char *content;
    const char *topic;
    const char *client_base;
    const char *client_name;
} mqtt_req_t;

typedef struct {
    struct 
    {
        char *name;
        char *manufacturer;
        char *model;
        char *sw_version;
        char *identifiers;
    } device;
    char *availability_topic;
    char *device_class;
    uint64_t expire_after;
    bool force_update;
    char *icon;
    char *name;
    char *state_topic;
    char *unique_id;
    char *unit_of_measurement;
    char *value_template;
} discovery_string_t;


typedef esp_err_t (*mqtt_callback_t)(mqtt_req_t *r);

esp_err_t mqtt_start(const char *mqtt_broker, const char *topic_base, const char *client_name);

esp_err_t mqtt_stop();

esp_err_t mqtt_subscribe(const char *topic, int qos, mqtt_callback_t callback);

esp_err_t mqtt_on_connect(mqtt_callback_t callback);

esp_err_t mqtt_availability(const char *topic, const char *connect_msg, const char *disconnect_msg);

esp_err_t mqtt_resp_sendstr(const mqtt_req_t* r, const char *topic, const char *str, int qos, bool retain);

esp_err_t mqtt_send_discovery_string(const char* topic, discovery_string_t discovery);