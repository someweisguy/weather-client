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
#include "i2c.h"

#define I2C_ADDR 		0x68
#define STATUS_REG 		0x0f
#define DATA_REG_START 	0x00

/**
 * Gets the time stored on the DS3231 as a Unix epoch.
 *
 * @return the time stored on the DS3231 as a Unix epoch
 */
time_t ds3231_get_time();

/**
 * Sets the time on the DS3231 using the current system time and resets the
 * oscillator reset flag.
 *
 * @note since the DS3231 only has second resolution, this method will block
 * up to 1 second to try to synchronize the DS3231 time to the system time
 * as close as possible.
 *
 * @return true on success, false on failure.
 */
bool ds3231_set_time();

/**
 * Checks whether or not the oscillator stop flag has been reset on the
 * DS3231 which indicates a power loss event.
 *
 * @return true if power was lost, or if there was an error communicating
 * with the DS3231.
 */
bool ds3231_lost_power();

#endif /* MAIN_HARDWARE_DS3231_DS3231_H_ */
