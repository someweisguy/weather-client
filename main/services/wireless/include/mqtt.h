/*
 * mqtt.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERVICES_MQTT_H_
#define MAIN_SERVICES_MQTT_H_

#include <cstring>

#include "logger.h"
#include "esp_system.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"


esp_err_t mqtt_connect(const char* mqtt_broker);
esp_err_t mqtt_stop();

esp_err_t mqtt_publish(const char* topic, const char *data);

#endif /* MAIN_SERVICES_MQTT_H_ */
