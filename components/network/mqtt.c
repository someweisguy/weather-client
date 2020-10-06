#include "mqtt.h"

#include "mqtt_client.h"

esp_err_t mqtt_start(const char* mqtt_broker)
{
    const esp_mqtt_client_config_t config = {
        .uri = mqtt_broker
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&config);
    // TODO register event handlers
    esp_mqtt_client_start(client);

    return ESP_OK;
}