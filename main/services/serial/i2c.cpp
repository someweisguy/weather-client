/*
 * i2c.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "i2c.h"
static const char* TAG { "i2c" };
static const i2c_port_t I2C_PORT { I2C_NUM_1 };

esp_err_t i2c_start() {
	i2c_config_t i2c_config;
	i2c_config.mode = I2C_MODE_MASTER;
	i2c_config.sda_io_num = PIN_NUM_SDA;
	i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_config.scl_io_num = PIN_NUM_SCL;
	i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_config.master.clk_speed = 100000;

	verbose(TAG, "Configuring and installing i2c port");
	ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &i2c_config));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, i2c_config.mode, 0, 0, 0));

	return ESP_OK;
}

esp_err_t i2c_stop() {
	verbose(TAG, "Stopping i2c");
	return i2c_driver_delete(I2C_PORT);
}

esp_err_t i2c_read(const uint8_t i2c_addr, const uint8_t i2c_reg, void* data_rd,
		const size_t size) {

	// guard clause
	if (size == 0)
		return ESP_OK;

	const i2c_cmd_handle_t cmd { i2c_cmd_link_create() };

	// Start the i2c master
	verbose(TAG, "Starting the i2c master");
	if (i2c_master_start(cmd) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Write device address
	verbose(TAG, "Writing the device address");
	if (i2c_master_write_byte(cmd, (i2c_addr << 1), 0x1) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Write register
	verbose(TAG, "Writing device register");
	if (i2c_master_write_byte(cmd, i2c_reg, 0x1) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Send repeated start
	verbose(TAG, "Sending repeated start");
	if (i2c_master_start(cmd) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Send device address and read data
	verbose(TAG, "Sending device address and reading data");
	if (i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_READ, 0x1) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}
	if (size > 1 && i2c_master_read(cmd, (uint8_t*) data_rd, size - 1,
			I2C_MASTER_ACK) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}
	if (i2c_master_read_byte(cmd, (uint8_t*) data_rd + size - 1,
			I2C_MASTER_NACK) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	verbose(TAG, "Stopping i2c master");
	if (i2c_master_stop(cmd) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	verbose(TAG, "Sending data in the queue");
	esp_err_t send_i2c_ret { i2c_master_cmd_begin(I2C_PORT, cmd, 1000 /
			portTICK_RATE_MS) };
	i2c_cmd_link_delete(cmd);
	return send_i2c_ret;
}

esp_err_t i2c_write(const uint8_t i2c_addr, const uint8_t i2c_reg,
		const void* data_wr, const size_t size) {

	// guard clause
	if (size == 0)
		return ESP_OK;

	// Create a command handle
	const i2c_cmd_handle_t cmd { i2c_cmd_link_create() };

	verbose(TAG, "Starting i2c master");
	if (i2c_master_start(cmd) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Send device address (indicating write) and register to be written
	verbose(TAG, "Sending device address");
	if (i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, 0x01) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Send register we want to write to
	verbose(TAG, "Sending the write register");
	if (i2c_master_write_byte(cmd, i2c_reg, 0x1) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Put the data to be written in the i2c queue
	verbose(TAG, "Putting data in i2c queue");
	if (i2c_master_write(cmd, (uint8_t*) data_wr, size, 0x1) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Stop the i2c master
	verbose(TAG, "Stopping i2c master");
	if (i2c_master_stop(cmd) != ESP_OK) {
		i2c_cmd_link_delete(cmd);
		return ESP_FAIL;
	}

	// Send the data that is in the queue
	verbose(TAG, "Sending data in the queue");
	const esp_err_t send_i2c_ret { i2c_master_cmd_begin(I2C_PORT, cmd, 1000 /
			portTICK_RATE_MS) };
	i2c_cmd_link_delete(cmd);
	return send_i2c_ret;
}
