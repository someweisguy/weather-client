#include "mqtt.h"

#include "wlan.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

static const char *TAG = "mqtt";

typedef struct
{
    const char *topic;
    int8_t qos;
    mqtt_callback_t cb;
} subscription_data_t;

static esp_mqtt_client_handle_t client = NULL;
static esp_mqtt_client_config_t config = {};

// mqtt is connected
static bool mqtt_is_connected = false;

static char *mqtt_topic_prefix = NULL;
static char *mqtt_client_name = NULL;

// list of topic subscriptions
static subscription_data_t subscriptions[10];
static size_t num_subscriptions = 0;

// list of on connect callbacks and the availability message
static mqtt_callback_t on_connects[10];
static size_t num_on_connects = 0;
static char *availability_message = NULL;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        for (int i = 0; i < num_subscriptions; ++i)
            esp_mqtt_client_subscribe(event->client, subscriptions[i].topic, subscriptions[i].qos);

        // send the connect message
        if (config.lwt_topic != NULL)
            esp_mqtt_client_publish(client, config.lwt_topic, availability_message, strlen(availability_message), 2, true);

        // do on connects
        mqtt_req_t r = {
            .client_base = mqtt_topic_prefix,
            .client_name = mqtt_client_name,
            .client = &client,
            .content_len = 0,
            .content = NULL,
            .topic = NULL};
        for (int i = 0; i < num_on_connects; ++i)
            on_connects[i](&r);
    }

    else if (event->event_id == MQTT_EVENT_DATA)
    {
        // get the topic name
        char topic[event->topic_len + 1]; // + null terminator
        memcpy(topic, event->topic, event->topic_len);
        topic[event->topic_len] = 0;

        // find the callback associated with the topic
        for (int i = 0; i < num_subscriptions; ++i)
        {
            if (strcmp(subscriptions[i].topic, topic) == 0)
            {
                mqtt_req_t r = {
                    .client_base = mqtt_topic_prefix,
                    .client_name = mqtt_client_name,
                    .client = &(event->client),
                    .content_len = event->data_len,
                    .content = event->data,
                    .topic = subscriptions[i].topic};
                esp_err_t err = subscriptions[i].cb(&r);
                if (err)
                    ESP_LOGW(TAG, "function callback on topic '%s' returned error", topic);
                return err;
            }
        }
    }

    else if (event->event_id == MQTT_EVENT_CONNECTED)
        mqtt_is_connected = true;

    else if (event->event_id == MQTT_EVENT_DISCONNECTED)
        mqtt_is_connected = false;

    return ESP_OK;
}

static void mqtt_starter(void *handler_args, esp_event_base_t base, int event_id, void *event_data)
{
    if (client != NULL)
        esp_mqtt_client_start(client);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, mqtt_starter);
}

esp_err_t mqtt_start(const char *mqtt_broker, const char *topic_base, const char *client_name)
{
    // config the mqtt client
    config.uri = mqtt_broker;
    config.event_handle = mqtt_event_handler;
    client = esp_mqtt_client_init(&config);

    mqtt_topic_prefix = (char *)topic_base;
    mqtt_client_name = (char *)client_name;

    // start the mqtt client if there is an internet connection
    wlan_data_t wlan_data = {};
    esp_err_t err = wlan_get_data(&wlan_data);
    if (err || wlan_data.ip.addr == 0)
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, mqtt_starter, NULL);
    else
        esp_mqtt_client_start(client);

    return ESP_OK;
}

esp_err_t mqtt_stop()
{
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    client = NULL;
    return ESP_OK;
}

esp_err_t mqtt_subscribe(const char *topic, int qos, mqtt_callback_t callback)
{
    // if mqtt is connected subscribe now
    if (mqtt_is_connected)
        esp_mqtt_client_subscribe(client, topic, qos);

    // add the subscription to the list
    if (num_subscriptions < sizeof(subscriptions) / sizeof(subscription_data_t))
    {
        subscriptions[num_subscriptions].topic = topic;
        subscriptions[num_subscriptions].qos = qos;
        subscriptions[num_subscriptions].cb = callback;
        num_subscriptions++;
        return ESP_OK;
    }
    else
        return ESP_ERR_INVALID_SIZE;
}

esp_err_t mqtt_on_connect(mqtt_callback_t callback)
{
    // if mqtt is connected do callback now
    if (mqtt_is_connected)
    {
        mqtt_req_t r = {
            .client_base = mqtt_topic_prefix,
            .client_name = mqtt_client_name,
            .client = &client,
            .content_len = 0,
            .content = NULL,
            .topic = NULL};
        callback(&r);
    }

    // add the subscription to the list
    if (num_on_connects < sizeof(on_connects) / sizeof(mqtt_callback_t))
    {
        on_connects[num_on_connects] = callback;
        num_on_connects++;
        return ESP_OK;
    }
    else
        return ESP_ERR_INVALID_SIZE;
}

esp_err_t mqtt_availability(const char *topic, const char *connect_msg, const char *disconnect_msg)
{
    availability_message = (char *)connect_msg;

    // update the config
    config.lwt_topic = topic;
    config.lwt_msg = disconnect_msg;
    config.lwt_retain = true;
    config.lwt_qos = 2;

    return esp_mqtt_set_config(client, &config);
}

esp_err_t mqtt_resp_sendstr(const mqtt_req_t *r, const char *topic, const char *str, int qos, bool retain)
{
    const int ret = esp_mqtt_client_publish(*(r->client), topic, str, strlen(str), qos, retain);
    return ret == -1 ? ESP_FAIL : ESP_OK;
}

esp_err_t mqtt_send_discovery_string(const char *topic, discovery_string_t discovery)
{
    cJSON *root = cJSON_CreateObject();

    // add device information
    cJSON *device = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(device, "name", discovery.device.name);
    cJSON_AddStringToObject(device, "manufacturer", discovery.device.manufacturer);
    cJSON_AddStringToObject(device, "model", discovery.device.model);
    cJSON_AddStringToObject(device, "sw_version", discovery.device.sw_version);
    // cJSON_AddStringToObject(device, "identifiers", __DATE__ __TIME__); TODO
    cJSON_AddStringToObject(device, "identifiers", discovery.device.identifiers); // del me

    // add entity information
    if (discovery.device_class != NULL)
        cJSON_AddStringToObject(root, "device_class", discovery.device_class);
    if (discovery.icon != NULL)
        cJSON_AddStringToObject(root, "icon", discovery.icon);
    cJSON_AddBoolToObject(root, "force_update", discovery.force_update);
    cJSON_AddStringToObject(root, "name", discovery.name);
    cJSON_AddStringToObject(root, "state_topic", discovery.state_topic);
    cJSON_AddStringToObject(root, "unique_id", discovery.unique_id);
    cJSON_AddStringToObject(root, "unit_of_measurement", discovery.unit_of_measurement);
    cJSON_AddStringToObject(root, "value_template", discovery.value_template);

    char *str = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(client, topic, str, strlen(str), 2, false);
    cJSON_Delete(root);
    free(str);
    return ESP_OK;
}