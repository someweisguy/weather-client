/*
 * bsp.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_BSP_H_
#define MAIN_BSP_H_

#include <cstdlib>
#include <cstdio>

#include "esp_system.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "driver/adc.h"

#include "logger.h"

enum wakeup_reason_t {
	UNEXPECTED_REASON,
	READY_SENSORS,
	TAKE_MEASUREMENT
};

struct config_t {
	char *wifi_ssid;
	char *wifi_password;
	char *mqtt_broker;
	char *mqtt_topic;
};

const char* esp_reset_to_name(esp_reset_reason_t code);

void strip(char *s);

esp_err_t sync_ntp_time(const char* ntp_server);

void set_cpu_time(const time_t unix_time);
time_t get_cpu_time();

int32_t get_line_length(FILE *f);

#endif /* MAIN_BSP_H_ */
