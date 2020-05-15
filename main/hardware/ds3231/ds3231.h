/*
 * ds3231.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HARDWARE_DS3231_DS3231_H_
#define MAIN_HARDWARE_DS3231_DS3231_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <ctime>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "i2c.h"

#define I2C_ADDR 		0x68
#define STATUS_REG 		0x0f
#define DATA_REG_START 	0x00


time_t ds3231_get_time();
bool ds3231_set_time();

bool ds3231_lost_power(bool &lost_power);

#endif /* MAIN_HARDWARE_DS3231_DS3231_H_ */
