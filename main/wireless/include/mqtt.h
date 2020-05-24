/*
 * mqtt.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_WIRELESS_MQTT_H_
#define MAIN_WIRELESS_MQTT_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstring>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "wlan.h"

#define MQTT_EVENT_ANY_ID ((esp_mqtt_event_id_t) -1)

#define ESP_ERR_MQTT_BASE         0xd000
#define ESP_ERR_MQTT_NOT_INIT    (0xd000 + 1)
#define ESP_ERR_MQTT_NOT_STARTED (0xd000 + 2)
#define ESP_ERR_MQTT_NOT_CONNECT (0xd000 + 3)

#define MQTT_MAX_RETRIES 5

#define INIT_BIT		BIT0
#define START_BIT 		BIT1
#define CONNECT_BIT 	BIT2
#define DISCONNECT_BIT	BIT3
#define STOP_BIT 		BIT4
#define PUBLISH_BIT 	BIT5
#define FAIL_BIT 		BIT6


void mqtt_connect(const char* mqtt_broker);
bool mqtt_block_until_connected(const time_t wait_millis = 0);
bool mqtt_connect_and_block(const char* mqtt_broker, const time_t wait_millis = 0);
bool mqtt_stop();

bool mqtt_initialized();
bool mqtt_started();
bool mqtt_connected();

esp_mqtt_client_handle_t &mqtt_get_client();

bool mqtt_publish(const char* topic, const char *data);

#endif /* MAIN_WIRELESS_MQTT_H_ */
