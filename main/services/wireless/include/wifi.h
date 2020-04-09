/*
 * wifi.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERVICES_WIFI_H_
#define MAIN_SERVICES_WIFI_H_

#include <cstring>

#include "../../logger/logger.h"
#include "esp_system.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"


esp_err_t wifi_connect(const char* ssid, const char* pass);
esp_err_t wifi_stop();

#endif /* MAIN_SERVICES_WIFI_H_ */
