/*
 * i2c.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERIAL_I2C_H_
#define MAIN_SERIAL_I2C_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstdint>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "helpers.h"

#define I2C_PORT 	I2C_NUM_1
#define PIN_NUM_SDA 23 // Adafruit Feather 32 Default
#define PIN_NUM_SCL 22 // Adafruit Feather 32 Default


bool i2c_start();
bool i2c_stop();

bool i2c_read(const char i2c_addr, const char i2c_reg, void* data_rd,
		const size_t size, const time_t wait_millis = 0);
bool i2c_write(const char i2c_addr, const char i2c_reg, const void* data_wr,
		const size_t size, const time_t wait_millis = 0);


#endif /* MAIN_SERIAL_I2C_H_ */
