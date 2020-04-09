/*
 * i2c.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERVICES_SERIAL_I2C_H_
#define MAIN_SERVICES_SERIAL_I2C_H_

#include "esp_system.h"
#include "driver/i2c.h"

#include "logger.h"

// Hardware pin defines
#define PIN_NUM_SDA (gpio_num_t) 23 // Adafruit Feather 32 Default
#define PIN_NUM_SCL (gpio_num_t) 22 // Adafruit Feather 32 Default

esp_err_t i2c_start();
esp_err_t i2c_stop();

esp_err_t i2c_read(const uint8_t i2c_addr, const uint8_t i2c_reg, void* data_rd,
		const size_t size);
esp_err_t i2c_write(const uint8_t i2c_addr, const uint8_t i2c_reg,
		const void* data_wr, const size_t size);



#endif /* MAIN_SERVICES_SERIAL_I2C_H_ */
