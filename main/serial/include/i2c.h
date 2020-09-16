#ifndef MAIN_SERIAL_I2C_H_
#define MAIN_SERIAL_I2C_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2c.h"

#define I2C_PORT 	I2C_NUM_1
#define PIN_NUM_SDA 23 // Adafruit Feather 32 Default
#define PIN_NUM_SCL 22 // Adafruit Feather 32 Default

/**
 * Start the I2C bus.
 *
 * @return true on success
 */
bool i2c_start();

/**
 * Stops the I2C bus and frees memory.
 *
 * @return true on success
 */
bool i2c_stop();

/**
 * Read from the I2C bus.
 *
 * @param address		the address of the desired I2C device
 * @param reg			the register to be read from the I2C device
 * @param rd			a pointer to the location to store the data that is read
 * @param size			the size in bytes to read
 * @param wait_ms		the amount of time in milliseconds to wait before timing out
 *
 * @return true on read success
 */
bool i2c_read(const char address, const char reg, void* rd,
		const size_t size, const time_t wait_ms);

/**
 * Write to the I2C bus.
 *
 * @param address		the address of the desired I2C device
 * @param reg			the register to be written from the I2C device
 * @param wr			a pointer to the location of the data to write
 * @param size			the size in bytes to write
 * @param wait_ms		the amount of time in milliseconds to wait before timing out
 */
bool i2c_write(const char address, const char reg, const void* wr,
		const size_t size, const time_t wait_ms);


#endif /* MAIN_SERIAL_I2C_H_ */
