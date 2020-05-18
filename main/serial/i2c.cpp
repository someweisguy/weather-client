/*
 * i2c.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "i2c.h"
static const char *TAG { "i2c" };

bool i2c_start() {
	// Configure the i2c port
	ESP_LOGV(TAG, "Configuring the I2C port");
	i2c_config_t i2c_config;
	i2c_config.mode = I2C_MODE_MASTER;
	i2c_config.master.clk_speed = 100000;
	i2c_config.sda_io_num = PIN_NUM_SDA;
	i2c_config.scl_io_num = PIN_NUM_SCL;
	i2c_config.sda_pullup_en = true;
	i2c_config.scl_pullup_en = true;
	esp_err_t config_ret { i2c_param_config(I2C_PORT, &i2c_config) };
	if (config_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to configure the I2C port (%i)", config_ret);
		return false;
	}

	// Install the driver
	ESP_LOGD(TAG, "Installing the I2C port driver");
	esp_err_t install_ret { i2c_driver_install(I2C_PORT, i2c_config.mode, 0,
			0, 0) };
	if (install_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to install the I2C port (%i)", install_ret);
		return false;
	}
	return true;
}

bool i2c_stop() {
	ESP_LOGD(TAG, "Stopping the I2C port driver");
	esp_err_t delete_ret { i2c_driver_delete(I2C_PORT) };
	if (delete_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to delete the I2C driver (%i)", delete_ret);
		return false;
	}
	return true;
}

bool i2c_read(const char address, const char reg, void* rd_buf,
		const size_t size, const time_t wait_millis) {
	if (size == 0)
		return true;

	// Create a command handle
	const i2c_cmd_handle_t cmd { i2c_cmd_link_create() };
	bool ret { false };

	do {
		// Start the i2c master
		ESP_LOGV(TAG, "Writing start condition");
		if (i2c_master_start(cmd) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write stop condition");
			break;
		}

		// Write device address
		ESP_LOGV(TAG, "Writing device address");
		if (i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE,
				true) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write device address");
			break;
		}

		// Write register
		ESP_LOGV(TAG, "Writing device register");
		if (i2c_master_write_byte(cmd, reg, true) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write device register");
			break;
		}

		// Send repeated start
		ESP_LOGV(TAG, "Writing repeated start");
		if (i2c_master_start(cmd) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to send repeated start");
			break;
		}

		// Send device address and read data
		ESP_LOGV(TAG, "Writing device address");
		if (i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ,
				true) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to send device address");
			break;
		}

		// Read the data and the terminator
		ESP_LOGV(TAG, "Writing read command");
		if (i2c_master_read(cmd, reinterpret_cast<uint8_t*>(rd_buf), size,
				I2C_MASTER_ACK) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write read command");
			break;
		}
		if (i2c_master_read_byte(cmd, reinterpret_cast<uint8_t*>(rd_buf) + size,
				I2C_MASTER_NACK) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write read terminator command");
			break;
		}

		// Stop the i2c master
		ESP_LOGV(TAG, "Writing stop condition");
		if (i2c_master_stop(cmd) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write stop condition");
			break;
		}

		// Get the amount of ticks to wait
		const TickType_t ticks { wait_millis == 0 ? portMAX_DELAY :
				wait_millis / portTICK_PERIOD_MS };

		// Send the data to the slave device
		ESP_LOGV(TAG, "Sending the data in the queue");
		esp_err_t send_ret { i2c_master_cmd_begin(I2C_PORT, cmd, ticks) };
		if (send_ret == ESP_ERR_INVALID_ARG)
			ESP_LOGE(TAG, "Unable to write to the I2C port (parameter error)");
		else if (send_ret == ESP_FAIL)
			ESP_LOGE(TAG, "Unable to write to the I2C port (no response)");
		else if (send_ret == ESP_ERR_INVALID_STATE)
			ESP_LOGE(TAG, "Unable to write to the I2C port (not installed)");
		else if (send_ret == ESP_ERR_TIMEOUT)
			ESP_LOGE(TAG, "Unable to write to the I2C port (timed out)");
		else {
			// Log results
			char hex_str[size * 5];
			strnhex(hex_str, reinterpret_cast<char*>(rd_buf), size);
			ESP_LOGD(TAG, "Got data from I2C 0x%02X, address 0x%02X: %s",
					address, reg, hex_str);
			ret = true;
		}
	} while (false);

	// Delete the i2c command and return results
	i2c_cmd_link_delete(cmd);
	return ret;
}

bool i2c_write(const char address, const char reg, const void* wr_buf,
		const size_t size, const time_t wait_millis) {
	if (size == 0)
		return true;

	// Log write buffer as hex string
	char hex_str[size * 5];
	strnhex(hex_str, reinterpret_cast<const char*>(wr_buf), size);
	ESP_LOGD(TAG, "Writing data to I2C 0x%02X, address 0x%02X: %s",
			address, reg, hex_str);

	// Create a command handle
	const i2c_cmd_handle_t cmd { i2c_cmd_link_create() };
	bool ret { false };

	do {
		ESP_LOGV(TAG, "Writing start condition");
		if (i2c_master_start(cmd) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write start condition");
			break;
		}

		// Send device address (indicating write) and register to be written
		ESP_LOGV(TAG, "Writing device address");
		if (i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE,
				true) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write device address");
			break;
		}

		// Send register we want to write to
		ESP_LOGV(TAG, "Writing device register");
		if (i2c_master_write_byte(cmd, reg, 0x1) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write device register");
			break;
		}

		// Put the data to be written in the i2c queue
		ESP_LOGV(TAG, "Writing data");
		if (i2c_master_write(cmd, (uint8_t*) wr_buf, size, 0x1) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write data");
			break;
		}

		// Stop the i2c master
		ESP_LOGV(TAG, "Writing stop condition");
		if (i2c_master_stop(cmd) != ESP_OK) {
			ESP_LOGE(TAG, "Unable to write stop condition");
			break;
		}

		// Get the amount of ticks to wait
		const TickType_t ticks { wait_millis == 0 ? portMAX_DELAY :
				wait_millis / portTICK_PERIOD_MS };

		// Send the data that is in the queue
		ESP_LOGV(TAG, "Sending data in the queue");
		esp_err_t send_ret { i2c_master_cmd_begin(I2C_PORT, cmd, ticks) };
		if (send_ret == ESP_ERR_INVALID_ARG)
			ESP_LOGE(TAG, "Unable to write to the I2C port (parameter error)");
		else if (send_ret == ESP_FAIL)
			ESP_LOGE(TAG, "Unable to write to the I2C port (no response)");
		else if (send_ret == ESP_ERR_INVALID_STATE)
			ESP_LOGE(TAG, "Unable to write to the I2C port (not installed)");
		else if (send_ret == ESP_ERR_TIMEOUT)
			ESP_LOGE(TAG, "Unable to write to the I2C port (timed out)");
		else
			ret = true;
	} while (false);

	// Delete the i2c command and return results
	i2c_cmd_link_delete(cmd);
	return ret;
}
