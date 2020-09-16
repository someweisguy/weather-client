#ifndef MAIN_SERIAL_I2C_H_
#define MAIN_SERIAL_I2C_H_

#include "esp_system.h"

esp_err_t i2c_start();

esp_err_t i2c_stop();

esp_err_t i2c_read(char addr, char reg, void *buf, size_t size, time_t wait_ms);

esp_err_t i2c_write(char addr, char reg, void *buf, size_t size, time_t wait_ms);

#endif /* MAIN_SERIAL_I2C_H_ */
