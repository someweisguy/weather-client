#include "mqtt.h"

#include "wlan.h"

#include "mqtt_client.h"

static esp_mqtt_client_handle_t client = NULL;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
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
    // TODO register event handlers

    // start the mqtt client if there is an internet connection
    wlan_data_t wlan_data = {};
    esp_err_t err = wlan_get_data(&wlan_data);
    if (err || wlan_data.ip.addr == 0)
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, mqtt_starter, NULL);
    else
        esp_mqtt_client_start(client);

    return ESP_OK;
}