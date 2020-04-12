/*
 * wifi.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "wifi.h"
static const char* TAG { "wifi" };
static const EventBits_t WIFI_SUCCESS { 0x1 }, WIFI_FAIL { 0x2 };
static const uint8_t WIFI_MAX_RETRIES { 5 };

static EventGroupHandle_t wifi_event_group;
static uint8_t wifi_connect_retry;

static void event_handler(void* handler_args, esp_event_base_t base,
		int event_id, void* event_data) {
	// Handle WiFi events
	if (base == WIFI_EVENT) {
		switch (event_id) {
		case WIFI_EVENT_STA_START:
			debug(TAG, "Handling WIFI_EVENT_STA_START event");
			wifi_connect_retry = 0;
			esp_wifi_connect();
			break;

		case WIFI_EVENT_STA_CONNECTED:
			debug(TAG, "Handling WIFI_EVENT_STA_CONNECTED event");
			break;

		case WIFI_EVENT_STA_DISCONNECTED:
			debug(TAG, "Handling WIFI_EVENT_STA_DISCONNECTED event");
			if (++wifi_connect_retry <= WIFI_MAX_RETRIES) {
				verbose(TAG, "Retrying connection");
				esp_wifi_connect();
			} else {
				xEventGroupSetBits(wifi_event_group, WIFI_FAIL);
			}
			break;

		default:
			debug(TAG, "Handling unexpected WiFi event (%i)", event_id);
			// Do nothing
			break;
		}
	}

	// Handle IP events
	else if (base == IP_EVENT) {
		//((ip_event_got_ip_t*) event_data)->ip_info.ip;
		xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
		wifi_connect_retry = 0;
	}
}

esp_err_t wifi_connect(const char* ssid, const char* pass) {

	// Create the WiFi event group
	wifi_event_group = xEventGroupCreate();

	verbose(TAG, "Configuring WiFi service");
	wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	wifi_config_t sta_config {};
	memcpy(sta_config.sta.ssid, ssid, 32);
	memcpy(sta_config.sta.password, pass, 64);
	sta_config.sta.bssid_set = false;

	// Register the WiFi event handler
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
			&event_handler, nullptr));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
			&event_handler, nullptr));

	verbose(TAG, "Attempting to connect to WiFi");
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	// Wait until WiFi connects or fails
	const EventBits_t wifi_ret { xEventGroupWaitBits(wifi_event_group,
			WIFI_SUCCESS | WIFI_FAIL, pdFALSE, pdFALSE, portMAX_DELAY) };

	if (wifi_ret == WIFI_SUCCESS) {
		return ESP_OK;
	} else {
		return ESP_FAIL;
	}
}

esp_err_t wifi_stop() {
	esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler);
	esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler);
	vEventGroupDelete(wifi_event_group);
	return esp_wifi_stop();
}
