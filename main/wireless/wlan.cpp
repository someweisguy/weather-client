/*
 * wifi.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "wlan.h"
static const char *TAG { "wlan" };
static EventGroupHandle_t wifi_event_group { xEventGroupCreate() };

static void event_handler(void *handler_args, esp_event_base_t base,
		int event_id, void *event_data) {

	// Handle WiFi events
	if (base == WIFI_EVENT) {
		switch (event_id) {
		case WIFI_EVENT_STA_START:
			ESP_LOGV(TAG, "Handling WIFI_EVENT_STA_START event");
			xEventGroupSetBits(wifi_event_group, START_BIT);
			xEventGroupClearBits(wifi_event_group, STOP_BIT);
			esp_wifi_connect();
			break;

		case WIFI_EVENT_STA_CONNECTED: {
			ESP_LOGV(TAG, "Handling WIFI_EVENT_STA_CONNECTED event");
			ESP_LOGI(TAG, "Connected to WiFi");
			break;
		}

		case WIFI_EVENT_STA_DISCONNECTED: {
			ESP_LOGV(TAG, "Handling WIFI_EVENT_STA_DISCONNECTED event");
			if (wlan_connected())
				ESP_LOGI(TAG, "Disconnected from WiFi");
			xEventGroupSetBits(wifi_event_group, DISCONNECT_BIT);
			xEventGroupClearBits(wifi_event_group, CONNECT_BIT);

			// Attempt to auto-reconnect
			const esp_err_t connect_ret { esp_wifi_connect() };
			if (connect_ret != ESP_OK && connect_ret != ESP_ERR_WIFI_NOT_STARTED)
				ESP_LOGE(TAG, "An error occurred while trying to connect (%i)",
						connect_ret);
		}
			break;

		case WIFI_EVENT_STA_STOP:
			ESP_LOGV(TAG, "Handling WIFI_EVENT_STA_STOP event");
			xEventGroupSetBits(wifi_event_group, STOP_BIT);
			xEventGroupClearBits(wifi_event_group, START_BIT);
			break;

		default:
			ESP_LOGW(TAG, "Handling unexpected event (%i)", event_id);
			break;
		}
	}

	// Handle IP events
	else if (base == IP_EVENT) {
		const ip_event_got_ip_t *event { (ip_event_got_ip_t*) event_data };
		ESP_LOGV(TAG, "Handling IP_EVENT_STA_GOT_IP event");
		ESP_LOGD(TAG, "Got IP '%d.%d.%d.%d'", IP2STR(&event->ip_info.ip));
		xEventGroupSetBits(wifi_event_group, CONNECT_BIT);
		xEventGroupClearBits(wifi_event_group, DISCONNECT_BIT);
	}
}

static void sntp_event_handler(timeval *tv) {
	const double epoch { tv->tv_sec + (tv->tv_usec / 1000000.0) };
	ESP_LOGV(TAG, "Received epoch from the SNTP server: %.3f", epoch);
	xEventGroupSetBits(wifi_event_group, SNTP_BIT);
}

void wlan_connect(const char *ssid, const char *pass) {
	if (wlan_connected()) {
		ESP_LOGD(TAG, "Already connected to WiFi");
		return;
	}

	if (!wlan_initialized()) {
		// Create default WiFi station
		esp_netif_create_default_wifi_sta();

		// Configure the service
		ESP_LOGV(TAG, "Configuring radio");
		wifi_init_config_t radio_config = WIFI_INIT_CONFIG_DEFAULT()
		;
		esp_err_t wifi_init_ret { esp_wifi_init(&radio_config) };
		if (wifi_init_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to initialize radio (%i)", wifi_init_ret);
			// Nothing to free
			return;
		}

		// Create the WiFi event group
		wifi_event_group = xEventGroupCreate();

		// Register the WiFi event handler
		ESP_LOGV(TAG, "Registering event handlers");
		char *handler_args { const_cast<char*>(ssid) };
		esp_err_t wifi_handler_ret { esp_event_handler_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID, event_handler, handler_args) };
		esp_err_t ip_handler_ret { esp_event_handler_register(IP_EVENT,
				IP_EVENT_STA_GOT_IP, event_handler, nullptr) };
		if (wifi_handler_ret != ESP_OK || ip_handler_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to register event handlers (wifi: %i, " "ip: %i)",
					wifi_handler_ret, ip_handler_ret);
			wlan_stop();
			return;
		}

		// Configure the connection
		ESP_LOGV(TAG, "Configuring connection options");
		wifi_config_t wifi_config { };
		memcpy(wifi_config.sta.ssid, ssid, 32);
		memcpy(wifi_config.sta.password, pass, 64);
		wifi_config.sta.bssid_set = false;
		esp_err_t mode_ret { esp_wifi_set_mode(WIFI_MODE_STA) };
		esp_err_t config_ret { esp_wifi_set_config(ESP_IF_WIFI_STA,
				&wifi_config) };
		if (mode_ret != ESP_OK || config_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to configure connection (mode: %i, " "config: %i)",
					mode_ret, config_ret);
			wlan_stop();
			return;
		}

		// Set the initialized bit
		ESP_LOGV(TAG, "Setting the initialized bit");
		xEventGroupSetBits(wifi_event_group, INIT_BIT);

		// Configure the SNTP service
		ESP_LOGV(TAG, "Configuring SNTP service");
		sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
		sntp_setoperatingmode(SNTP_OPMODE_POLL);
		sntp_setservername(0, "0.pool.ntp.org");
		sntp_setservername(1, "1.pool.ntp.org");
		sntp_setservername(2, "2.pool.ntp.org");
		sntp_setservername(3, "3.pool.ntp.org");
		sntp_set_time_sync_notification_cb(&sntp_event_handler);
	}

	if (!wlan_started()) {
		ESP_LOGI(TAG, "Connecting to WiFi SSID '%s'...", ssid);
		esp_err_t wifi_start_ret { esp_wifi_start() };
		if (wifi_start_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to start WiFi (%i)", wifi_start_ret);
			wlan_stop();
		} else {
			// Report service started without having to wait for scheduler
			xEventGroupSetBits(wifi_event_group, START_BIT);
			xEventGroupClearBits(wifi_event_group, STOP_BIT);
		}
	} else {
		ESP_LOGI(TAG, "Reconnecting to WiFi...");
		esp_err_t connect_ret { esp_wifi_connect() };
		if (connect_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to connect to WiFi (%i)", connect_ret);
			wlan_stop();
		}
	}
	xEventGroupClearBits(wifi_event_group, CONNECT_BIT | DISCONNECT_BIT);
}

bool wlan_block_until_connected(const time_t wait_millis) {
	if (!wlan_started()) {
		ESP_LOGE(TAG, "Unable to wait for WiFi connection (not started)");
		return false;
	}

	// Get the amount of ticks to wait
	const TickType_t ticks { wait_millis == 0 ?
			portMAX_DELAY : wait_millis / portTICK_PERIOD_MS };

	// Block until a result is returned
	const EventBits_t wifi_ret { xEventGroupWaitBits(wifi_event_group,
	CONNECT_BIT | STOP_BIT, pdFALSE, pdFALSE, ticks) };

	// Return the results
	if (wifi_ret & CONNECT_BIT)
		return true;
	else {
		if (!(wifi_ret & STOP_BIT))
			ESP_LOGW(TAG, "Unable to wait for Wifi connection (timed out)");
		return false;
	}
}

bool wlan_connect_and_block(const char *ssid, const char *pass,
		const time_t wait_millis) {
	wlan_connect(ssid, pass);
	return wlan_block_until_connected(wait_millis);
}

bool wlan_stop() {
	// Unregister the event handler
	ESP_LOGV(TAG, "Unregistering event handler");
	esp_err_t unregister_ret { esp_event_handler_unregister(ESP_EVENT_ANY_BASE,
			ESP_EVENT_ANY_ID, event_handler) };
	if (unregister_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to unregister event handler (%i)",
				unregister_ret);
		return false;
	}

	// Disconnect WiFi if it is connected
	if (wlan_connected()) {
		ESP_LOGI(TAG, "Disconnecting from WiFi");
		esp_err_t wifi_disconnect_ret { esp_wifi_disconnect() };
		if (wifi_disconnect_ret != ESP_OK) {
			ESP_LOGW(TAG, "Unable to disconnect from WiFi (%i)",
					wifi_disconnect_ret);
		} else {
			// Wait for WiFi to stop
			xEventGroupWaitBits(wifi_event_group, STOP_BIT, pdFALSE, pdFALSE,
					portMAX_DELAY);
		}
	}


	// Free the memory allocated for WiFi and return the results
	ESP_LOGD(TAG, "Freeing memory");
	esp_err_t deinit_ret { esp_wifi_deinit() };
	if (deinit_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to deinitialize WiFi (%i)", deinit_ret);
		return false;
	}

	// Clear the initialized bit
	ESP_LOGV(TAG, "Clearing the initialized bit");
	xEventGroupClearBits(wifi_event_group, INIT_BIT);

	return true;
}

bool wlan_initialized() {
	return xEventGroupGetBits(wifi_event_group) & INIT_BIT;
}

bool wlan_started() {
	return wlan_initialized()
			&& (xEventGroupGetBits(wifi_event_group) & START_BIT);
}

bool wlan_connected() {
	return wlan_started()
			&& (xEventGroupGetBits(wifi_event_group) & CONNECT_BIT);
}

bool sntp_synchronize_system_time(const char* timezone_str,
		const time_t wait_millis) {
	if (!wlan_initialized()) {
		ESP_LOGE(TAG, "Unable to synchronize system time with the SNTP server (not initialized)");
		return false;
	} else if (!wlan_connected()) {
		ESP_LOGE(TAG, "Unable to synchronize system time with the SNTP server (not connected)");
		return false;
	}

	ESP_LOGI(TAG, "Synchronizing system time with SNTP server...");
	sntp_init();

	// Get the amount of ticks to wait
	const TickType_t ticks {
			wait_millis == 0 ? portMAX_DELAY : wait_millis / portTICK_PERIOD_MS };

	// Wait to connect to NTP server
	const EventBits_t sntp_ret { xEventGroupWaitBits(wifi_event_group,
			SNTP_BIT | STOP_BIT, pdFALSE, pdFALSE, ticks) };
	sntp_stop();

	if (sntp_ret & STOP_BIT) {
		ESP_LOGE(TAG, "Unable to synchronize system time with the SNTP server (disconnected)");
		return false;
	} else if (!(sntp_ret & SNTP_BIT)) {
		ESP_LOGE(TAG, "Unable to synchronize system time with the SNTP server (timed out)");
		return false;
	} else {
		if (timezone_str != nullptr) {
			setenv("TZ", timezone_str, 1);
			tzset();
		}
		ESP_LOGI(TAG, "Synchronized system time");
		xEventGroupClearBits(wifi_event_group, SNTP_BIT);
		return true;
	}
}
