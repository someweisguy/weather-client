/*
 * mqtt.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "mqtt.h"

static const char* TAG { "mqtt" };
static const esp_mqtt_event_id_t MQTT_EVENT_ANY_ID { static_cast<esp_mqtt_event_id_t>(-1) };
static const EventBits_t MQTT_SUCCESS { 0x1 }, MQTT_FAIL { 0x2 },
	MQTT_PUBLISH { 0x4 };
static EventGroupHandle_t mqtt_event_group;
static esp_mqtt_client_handle_t client;
static volatile bool connected { false };


static void event_handler(void *handler_args, esp_event_base_t base,
		int32_t event_id, void *event_data) {

	esp_mqtt_event_handle_t mqtt_event {
			static_cast<esp_mqtt_event_handle_t>(event_data) };

	// Handle mqtt events
	switch (event_id) {
	case MQTT_EVENT_CONNECTED:
		debug(TAG, "Handling MQTT_EVENT_CONNECTED event");
		connected = true;
		xEventGroupSetBits(mqtt_event_group, MQTT_SUCCESS);
		break;

	case MQTT_EVENT_DISCONNECTED:
		debug(TAG, "Handling MQTT_EVENT_DISCONNECTED event");
		connected = false;
		xEventGroupSetBits(mqtt_event_group, MQTT_FAIL);
		break;

	case MQTT_EVENT_ERROR:
		debug(TAG, "Handling MQTT_EVENT_ERROR event");
		debug(TAG, "Got MQTT error type (%i) and connection code (%i)",
				mqtt_event->error_handle->error_type,
				mqtt_event->error_handle->connect_return_code);
		xEventGroupSetBits(mqtt_event_group, MQTT_FAIL);
		break;

	case MQTT_EVENT_PUBLISHED:
		verbose(TAG, "Handling MQTT_EVENT_PUBLISHED event");
		xEventGroupSetBits(mqtt_event_group, MQTT_PUBLISH);
		break;

	case MQTT_EVENT_DATA:
		verbose(TAG, "Handling MQTT_EVENT_DATA event");
		// Do nothing
		break;

	case MQTT_EVENT_BEFORE_CONNECT:
		verbose(TAG, "Handling MQTT_EVENT_BEFORE_CONNECT event");
		// Do nothing
		break;

	default:
		warning(TAG, "Handling unexpected MQTT event (%i)",
				mqtt_event->event_id);
		xEventGroupSetBits(mqtt_event_group, MQTT_FAIL);
		break;
	}

}


esp_err_t mqtt_connect(const char* mqtt_broker) {

	// Construct the event group
	mqtt_event_group = xEventGroupCreate();

	verbose(TAG, "Configuring MQTT service");
	esp_mqtt_client_config_t mqtt_config {};
	client = esp_mqtt_client_init(&mqtt_config);
	esp_mqtt_client_register_event(client, MQTT_EVENT_ANY_ID, event_handler,
			client);
	ESP_ERROR_CHECK(esp_mqtt_client_set_uri(client, mqtt_broker));
	verbose(TAG, "Attempting to connect to MQTT broker");
	ESP_ERROR_CHECK(esp_mqtt_client_start(client));

	// Wait for connection success or failure
	const EventBits_t mqtt_ret { xEventGroupWaitBits(mqtt_event_group,
			MQTT_SUCCESS | MQTT_FAIL, pdTRUE, pdFALSE, portMAX_DELAY) };

	if (mqtt_ret == MQTT_SUCCESS) {
		return ESP_OK;
	} else {
		return ESP_FAIL;
	}
}

esp_err_t mqtt_stop() {
	vEventGroupDelete(mqtt_event_group);
	if (esp_mqtt_client_destroy(client) != ESP_OK)
		return ESP_FAIL;
	return ESP_OK;
}

bool mqtt_connected() {
	return connected;
}

esp_err_t mqtt_publish(const char* topic, const char* data) {
	verbose(TAG, "Publishing MQTT data to topic \"%s\"", topic);
	esp_mqtt_client_publish(client, topic, data, 0, 2, 0);

	// Wait for the message to be published
	const EventBits_t publish_ret { xEventGroupWaitBits(mqtt_event_group,
			MQTT_PUBLISH | MQTT_FAIL, pdTRUE, pdFALSE, portMAX_DELAY) };

	if (publish_ret == MQTT_PUBLISH) {
		return ESP_OK;
	} else {
		return ESP_FAIL;
	}

}
