/*
 * mqtt.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "mqtt.h"
static const char *TAG { "mqtt" };
static EventGroupHandle_t mqtt_event_group { xEventGroupCreate() };
static esp_mqtt_client_handle_t client;

static void event_handler(void *handler_args, esp_event_base_t base,
		int32_t event_id, void *event_data) {

	esp_mqtt_event_handle_t mqtt_event { static_cast<esp_mqtt_event_handle_t>(
			event_data) };

	// Handle mqtt events
	switch (event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGV(TAG, "Handling MQTT_EVENT_CONNECTED event");
		ESP_LOGI(TAG, "Connected to MQTT broker");
		xEventGroupSetBits(mqtt_event_group, CONNECT_BIT);
		xEventGroupClearBits(mqtt_event_group, DISCONNECT_BIT);
		break;

	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGV(TAG, "Handling MQTT_EVENT_DISCONNECTED event");
		if (mqtt_connected())
			ESP_LOGI(TAG, "Disconnected from MQTT broker");
		xEventGroupSetBits(mqtt_event_group, DISCONNECT_BIT);
		xEventGroupClearBits(mqtt_event_group, CONNECT_BIT);

		// Stop MQTT if WiFi is not started
		if (!wlan_started()) {
			xEventGroupSetBits(mqtt_event_group, STOP_BIT);
			xEventGroupClearBits(mqtt_event_group, START_BIT);
			ESP_LOGE(TAG, "Unable to connect to MQTT broker (type: %i, conn: %i)",
					mqtt_event->error_handle->error_type,
					mqtt_event->error_handle->connect_return_code);
			mqtt_stop();
		}
		break;

	case MQTT_EVENT_ERROR:
		ESP_LOGV(TAG, "Handling MQTT_EVENT_ERROR event");
		// Return an error if we are not handling a connection error event
		if (mqtt_event->error_handle->error_type != MQTT_ERROR_TYPE_ESP_TLS) {
			ESP_LOGE(TAG, "An unexpected error occurred (type: %i, conn: %i)",
					mqtt_event->error_handle->error_type,
					mqtt_event->error_handle->connect_return_code);
			xEventGroupSetBits(mqtt_event_group, FAIL_BIT);
		}

		break;

	case MQTT_EVENT_PUBLISHED:
		ESP_LOGV(TAG, "Handling MQTT_EVENT_PUBLISHED event");
		ESP_LOGD(TAG, "MQTT message number %i was delivered", mqtt_event->msg_id);
		xEventGroupSetBits(mqtt_event_group, PUBLISH_BIT);
		break;

	case MQTT_EVENT_DATA:
		ESP_LOGV(TAG, "Handling MQTT_EVENT_DATA event");
		// Do nothing
		break;

	case MQTT_EVENT_BEFORE_CONNECT:
		ESP_LOGV(TAG, "Handling MQTT_EVENT_BEFORE_CONNECT event");
		xEventGroupSetBits(mqtt_event_group, START_BIT);
		xEventGroupClearBits(mqtt_event_group, STOP_BIT);
		break;

	default:
		ESP_LOGW(TAG, "Handling unexpected MQTT event (%i)", mqtt_event->event_id);
		break;
	}

}

void mqtt_connect(const char* mqtt_broker) {
	if (mqtt_connected()) {
		ESP_LOGD(TAG, "Already connected to MQTT");
		return;
	}

	if (!mqtt_initialized()) {
		// Turn off log messages - we will handle all relevant errors
		ESP_LOGV(TAG, "Turning off 'MQTT_CLIENT' log messages");
		esp_log_level_set("MQTT_CLIENT", ESP_LOG_NONE);

		// Configure the MQTT client
		ESP_LOGD(TAG, "Initializing the MQTT client");
		esp_mqtt_client_config_t mqtt_config {};
		client = esp_mqtt_client_init(&mqtt_config);
		if (client == nullptr) {
			ESP_LOGE(TAG, "Unable to initialize the MQTT client");
			return;
		}

		// Construct the event group
		mqtt_event_group = xEventGroupCreate();

		// Register event handler
		ESP_LOGV(TAG, "Registering event handler");
		esp_err_t register_ret { esp_mqtt_client_register_event(client,
				MQTT_EVENT_ANY_ID, event_handler, nullptr) };
		if (register_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to register the event handler (%i)",
					register_ret);
			mqtt_stop();
			return;
		}

		// Set the MQTT connection uri
		ESP_LOGV(TAG, "Setting connection URI");
		esp_err_t uri_ret { esp_mqtt_client_set_uri(client, mqtt_broker) };
		if (uri_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to set connection URI (%i)", uri_ret);
			mqtt_stop();
			return;
		}

		// Set the initialized bit
		ESP_LOGV(TAG, "Setting the initialized bit");
		xEventGroupSetBits(mqtt_event_group, INIT_BIT);
	}

	if (!mqtt_started()) {
		ESP_LOGI(TAG, "Connecting to the MQTT broker '%s'...", mqtt_broker);
		esp_err_t start_ret { esp_mqtt_client_start(client) };
		if (start_ret != ESP_OK) {
			ESP_LOGW(TAG, "Unable to start the MQTT client (%i)", start_ret);
		} else {
			// Report service started without having to wait for scheduler
			xEventGroupSetBits(mqtt_event_group, START_BIT);
			xEventGroupClearBits(mqtt_event_group, STOP_BIT);
		}
	} else {
		ESP_LOGI(TAG, "Reconnecting the MQTT client...");
		esp_err_t reconnect_ret { esp_mqtt_client_reconnect(client) };
		if (reconnect_ret != ESP_OK) {
			ESP_LOGW(TAG, "Unable to reconnect the MQTT client (%i)",
					reconnect_ret);
		}
	}

	xEventGroupClearBits(mqtt_event_group, CONNECT_BIT | DISCONNECT_BIT);
}

bool mqtt_block_until_connected(const time_t wait_millis) {
	if (!mqtt_started()) {
		ESP_LOGE(TAG, "Cannot wait for connection because the MQTT client "
				"wasn't started");
		return false;
	}

	// Get the amount of ticks to wait
	const TickType_t ticks { wait_millis == 0 ? portMAX_DELAY :
			wait_millis / portTICK_PERIOD_MS };

	// Block until a result is returned
	const EventBits_t mqtt_ret { xEventGroupWaitBits(mqtt_event_group,
			CONNECT_BIT | STOP_BIT, pdFALSE, pdFALSE, ticks) };

	// Return the results
	if (mqtt_ret & CONNECT_BIT)
		return true;
	else {
		if (!(mqtt_ret & STOP_BIT))
			ESP_LOGE(TAG, "Unable to connect to the MQTT broker (timed out)");
		return false;
	}
}

bool mqtt_connect_and_block(const char *mqtt_broker,
		const time_t wait_millis) {
	mqtt_connect(mqtt_broker);
	return mqtt_block_until_connected(wait_millis);
}

bool mqtt_stop() {
	// Stop the MQTT client
	ESP_LOGD(TAG, "Stopping the MQTT client");
	esp_mqtt_client_stop(client); // fail silently

	// Destroy the MQTT client
	ESP_LOGV(TAG, "Deinitializing the MQTT client");
	esp_err_t destroy_ret { esp_mqtt_client_destroy(client) };
	if (destroy_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to deinitialize the MQTT client (%i)",
				destroy_ret);
		return false;
	}

	// Clear the initialized bit
	ESP_LOGV(TAG, "Clearing the initialized bit");
	xEventGroupClearBits(mqtt_event_group, INIT_BIT);

	return true;
}

bool mqtt_initialized() {
	return xEventGroupGetBits(mqtt_event_group) & INIT_BIT;
}

bool mqtt_started() {
	return mqtt_initialized() && (xEventGroupGetBits(mqtt_event_group)
			& START_BIT);
}

bool mqtt_connected() {
	return mqtt_started() && (xEventGroupGetBits(mqtt_event_group)
			& CONNECT_BIT);
}

esp_mqtt_client_handle_t &mqtt_get_client() {
	return client;
}

bool mqtt_publish(const char* topic, const char* data) {
	if (!mqtt_connected()) {
		ESP_LOGE(TAG, "Unable to publish data to the MQTT broker (not connected)");
		return false;
	}

	// Clear the publish bit before publishing
	xEventGroupClearBits(mqtt_event_group, PUBLISH_BIT);

	ESP_LOGV(TAG, "Publishing MQTT message to topic '%s'", topic);
	const int msg_id { esp_mqtt_client_publish(client, topic, data, 0, 2, 0) };
	if (msg_id == -1) {
		ESP_LOGE(TAG, "Unable to publish to MQTT broker (can't queue)");
		return false;
	} else if (msg_id == 0) {
		// Don't wait for delivery confirmation
		return true;
	}

	// Wait for the message to be published
	const EventBits_t publish_ret { xEventGroupWaitBits(mqtt_event_group,
			PUBLISH_BIT | FAIL_BIT | DISCONNECT_BIT, pdFALSE, pdFALSE, portMAX_DELAY) };

	// Return the results
	if (publish_ret & PUBLISH_BIT)
		return true;
	else if (publish_ret & DISCONNECT_BIT)
		ESP_LOGE(TAG, "Unable to publish data to the MQTT broker (disconnected)");
	else if (publish_ret & FAIL_BIT)
		ESP_LOGE(TAG, "Unable to publish data to the MQTT broker (failed)");
	else
		ESP_LOGW(TAG, "Unable to publish data to the MQTT broker (timed out)");
	return false;
}
