/*
 * wlan.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERVICES_WIFI_H_
#define MAIN_SERVICES_WIFI_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstring>
#include <cstdlib>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_sntp.h"

#define START_BIT 		BIT0
#define CONNECT_BIT 	BIT1
#define DISCONNECT_BIT 	BIT2
#define STOP_BIT 		BIT3
#define SNTP_BIT 		BIT4


void wlan_connect(const char *ssid, const char *pass);
bool wlan_block_until_connected(const time_t wait_millis = 0);
bool wlan_connect_and_block(const char* ssid, const char* pass, const time_t wait_millis = 0);
bool wlan_stop();

bool wlan_initialized();
bool wlan_started();
bool wlan_connected();

bool sntp_synchronize_system_time(const char* timezone_str,
		const time_t wait_millis = 0);

#endif /* MAIN_SERVICES_WIFI_H_ */
