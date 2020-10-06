#include "mqtt.h"

#include "wlan.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt";

typedef struct
{
    const char *topic;
    int8_t qos;
    mqtt_callback_t cb;
} subscription_data_t;

static esp_mqtt_client_handle_t client = NULL;
static subscription_data_t subscriptions[10];
static size_t num_subscriptions = 0;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        for (int i = 0; i < num_subscriptions; ++i)
            esp_mqtt_client_subscribe(event->client, subscriptions[i].topic, subscriptions[i].qos);
    }

    else if (event->event_id == MQTT_EVENT_DATA)
    {
        for (int i = 0; i < num_subscriptions; ++i)
        {
            if (strcmp(subscriptions[i].topic, event->topic) == 0)
            {
                mqtt_req_t r = {
                    .client = &(event->client),
                    .content_len = event->data_len,
                    .content = event->data,
                    .topic = subscriptions[i].topic};
                esp_err_t err = subscriptions[i].cb(&r);
                if (err)
                    ESP_LOGW(TAG, "function callback on topic '%s' returned error", event->topic);
                return err;
            }
        }
    }

    return ESP_OK;
}

static void mqtt_starter(void *handler_args, esp_event_base_t base, int event_id, void *event_data)
{
    if (client != NULL)
        esp_mqtt_client_start(client);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, mqtt_starter);
}

esp_err_t mqtt_start(const char *mqtt_broker)
{

    const esp_mqtt_client_config_t config = {
        .uri = mqtt_broker,
        .event_handle = mqtt_event_handler};
    client = esp_mqtt_client_init(&config);

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
    if (client != NULL)
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

esp_err_t mqtt_resp_sendstr(const mqtt_req_t* r, const char *str, int qos, bool retain)
{
    return esp_mqtt_client_publish(*(r->client), r->topic, str, strlen(str), qos, retain);
}