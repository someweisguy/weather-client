/*
 * mqtt.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "mqtt.h"

static const char* TAG { "mqtt" };
static const EventBits_t MQTT_SUCCESS { 0x1 }, MQTT_FAIL { 0x2 },
	MQTT_PUBLISH { 0x4 };
static EventGroupHandle_t mqtt_event_group;
esp_mqtt_client_handle_t client;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t mqtt_event) {

	// Handle mqtt events
	switch (mqtt_event->event_id) {
	case MQTT_EVENT_CONNECTED:
		debug(TAG, "Handling MQTT_EVENT_CONNECTED event");
		xEventGroupSetBits(mqtt_event_group, MQTT_SUCCESS);
		break;

	case MQTT_EVENT_DISCONNECTED:
		debug(TAG, "Handling MQTT_EVENT_DISCONNECTED event");
		xEventGroupSetBits(mqtt_event_group, MQTT_FAIL);
		break;

	case MQTT_EVENT_ERROR:
		debug(TAG, "Handling MQTT_EVENT_ERROR event");
		esp_mqtt_client_stop(mqtt_event->client);
		xEventGroupSetBits(mqtt_event_group, MQTT_FAIL);
		break;

	case MQTT_EVENT_PUBLISHED:
		debug(TAG, "Handling MQTT_EVENT_PUBLISHED event");
		xEventGroupSetBits(mqtt_event_group, MQTT_PUBLISH);
		break;

	case MQTT_EVENT_DATA:
		debug(TAG, "Handling MQTT_EVENT_DATA event");
		// Do nothing
		break;

	case MQTT_EVENT_BEFORE_CONNECT:
		debug(TAG, "Handling MQTT_EVENT_BEFORE_CONNECT event");
		// Do nothing
		break;

	default:
		warning(TAG, "Handling unexpected MQTT event (%i)",
				mqtt_event->event_id);
		break;
	}

	// Handle MQTT connection errors
	// Doesn't seem to be implemented in ESP-IDF v4.0
	if (mqtt_event->error_handle->connect_return_code != MQTT_CONNECTION_ACCEPTED) {
		error(TAG, "Got MQTT connection error (%i)", mqtt_event->
				error_handle->connect_return_code);
		xEventGroupSetBits(mqtt_event_group, MQTT_FAIL);
	}

	// Handle MQTT general errors
	// Doesn't seem to be implemented in ESP-IDF v4.0
	if (mqtt_event->error_handle->error_type != MQTT_ERROR_TYPE_NONE) {
		error(TAG, "Got MQTT general error (%i)", mqtt_event->error_handle->error_type);
		xEventGroupSetBits(mqtt_event_group, MQTT_FAIL);
	}

	return ESP_OK;
}

esp_err_t mqtt_connect(const char* mqtt_broker) {

	// Construct the event group
	mqtt_event_group = xEventGroupCreate();

	verbose(TAG, "Configuring MQTT service");
	esp_mqtt_client_config_t mqtt_config {};
	mqtt_config.event_handle = &mqtt_event_handler;
	client = esp_mqtt_client_init(&mqtt_config);
	ESP_ERROR_CHECK(esp_mqtt_client_set_uri(client, mqtt_broker));
	verbose(TAG, "Attempting to connect to MQTT broker");
	ESP_ERROR_CHECK(esp_mqtt_client_start(client));

	// Wait 5000ms for MQTT to connect or fail - there is no MQTT fail event so
	//  if we don't connect in 5000ms, we are not connecting at all.
	const EventBits_t mqtt_ret { xEventGroupWaitBits(mqtt_event_group,
			MQTT_SUCCESS | MQTT_FAIL, pdTRUE, pdFALSE,
			5000 / portTICK_PERIOD_MS) };

	if (mqtt_ret == MQTT_SUCCESS) {
		return ESP_OK;
	} else {
		return ESP_FAIL;
	}
}

esp_err_t mqtt_stop() {
	vEventGroupDelete(mqtt_event_group);
	// The code below doesn't seem to be working properly in ESP-IDF v4.0
	/*
	if (esp_mqtt_client_stop(client) != ESP_OK)
		return ESP_FAIL;
	*/
	if (esp_mqtt_client_destroy(client) != ESP_OK)
		return ESP_FAIL;
	return ESP_OK;
}

esp_err_t mqtt_publish(const char* topic, const char* data) {
	verbose(TAG, "Publishing MQTT data to topic \"%s\"", topic);
	esp_mqtt_client_publish(client, topic, data, 0, 2, 0);

	// Wait for the message to be published
	const EventBits_t publish_ret { xEventGroupWaitBits(mqtt_event_group,
			MQTT_PUBLISH, pdTRUE, pdFALSE, portMAX_DELAY) };

	if (publish_ret == MQTT_PUBLISH) {
		return ESP_OK;
	} else {
		return ESP_FAIL;
	}

}
