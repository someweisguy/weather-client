/*
 * ds3231.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HARDWARE_DS3231_DS3231_H_
#define MAIN_HARDWARE_DS3231_DS3231_H_

#include <time.h>

#include "esp_system.h"
#include "i2c.h"

#include "logger.h"

esp_err_t ds3231_lost_power(bool &lost_power);

esp_err_t ds3231_get_time(time_t &unix_time);
esp_err_t ds3231_set_time(const time_t unix_time);

esp_err_t ds3231_get_temperature_celsius(float &temperature_C);

#endif /* MAIN_HARDWARE_DS3231_DS3231_H_ */
