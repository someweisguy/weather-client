/*
 * main.h
 *
 *  Created on: May 17, 2020
 *      Author: Mitch
 */

#ifndef MAIN_H_
#define MAIN_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstdio>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"

#include "i2c.h"
#include "uart.h"
#include "ds3231.h"
#include "sdcard.h"

#include "wlan.h"
#include "http.h"
#include "mqtt.h"

#include "helpers.h"

#include "Sensor.h"
#include "BME280.h"
#include "PMS5003.h"

#define SENSOR_READY_MS 30 * 1000 // 30 seconds

#define LOG_FILE_PATH       "/sdcard/events.log"
#define CONFIG_FILE_PATH    "/sdcard/config.json"
#define DATA_FILE_PATH      "/sdcard/data.txt"


void setup_required_services();
time_t get_window_wait_ms(const int modifier_ms);
void synchronize_system_time_task(void *args);
void sensor_sleep_task(void *args);
void send_backlog_task(void *args);

#endif /* MAIN_H_ */
