/*
 *
 * uart.cpp
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#include "uart.h"
static const char *TAG { "uart" };

bool uart_start() {
	// Configure the UART port
	ESP_LOGV(TAG, "Configuring the UART port");
	uart_config_t uart_config;
	uart_config.baud_rate = 9600;
	uart_config.data_bits = UART_DATA_8_BITS;
	uart_config.parity = UART_PARITY_DISABLE;
	uart_config.stop_bits = UART_STOP_BITS_1;
	uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uart_config.rx_flow_ctrl_thresh = 0;
	esp_err_t config_ret { uart_param_config(UART_PORT, &uart_config) };
	if (config_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to configure the UART port (%i)", config_ret);
		return false;
	}

	// Set the UART pin
	ESP_LOGV(TAG, "Setting the UART pin");
	esp_err_t set_pin_ret { uart_set_pin(UART_PORT, PIN_NUM_TX, PIN_NUM_RX,
			UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) };
	if (set_pin_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to set the UART pin number (%i)", set_pin_ret);
		return false;
	}

	// Install the driver
	ESP_LOGD(TAG, "Installing UART driver");
	QueueHandle_t uart_queue;
	esp_err_t install_ret { uart_driver_install(UART_PORT, BUF_SIZE * 2,
			BUF_SIZE * 2, 10, &uart_queue, 0) };
	if (install_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to install UART driver (%i)", install_ret);
		return false;
	}
	return true;
}

bool uart_stop() {
	// Flush the UART port
	ESP_LOGV(TAG, "Flushing the UART port");
	esp_err_t flush_ret { uart_flush(UART_PORT) };
	if (flush_ret != ESP_OK)
		ESP_LOGW(TAG, "Unable to flush UART port (%x)", flush_ret);

	// Delete the driver
	ESP_LOGD(TAG, "Deleting the UART driver");
	esp_err_t delete_ret { uart_driver_delete(UART_PORT) };
	if (delete_ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to delete the UART driver (%x)", delete_ret);
	return delete_ret;
}

int uart_write(const char *data_wr, const size_t size, const time_t wait_millis) {
	if (size == 0)
		return 0;

	// Send the data on the UART bus
	char hex_wr[size * 5];
	ESP_LOGD(TAG, "Writing data to UART: [ %s ]", strhex(hex_wr, data_wr));
	const int written { uart_write_bytes(UART_PORT, (char*) data_wr, size) };

	// Check number of bytes written
	if (written == -1) {
		ESP_LOGE(TAG, "Unable to write data to UART (parameter error)");
		return -1;
	} else if (written != size) {
		ESP_LOGE(TAG, "Unable to write data to UART (expected %u bytes but "
				"wrote %u)", size, written);
	}

	// Get the amount of ticks to wait
	const TickType_t ticks { wait_millis == 0 ? portMAX_DELAY :
			wait_millis / portTICK_PERIOD_MS };

	// Wait for the UART to flush transmission
	esp_err_t tx_ret { uart_wait_tx_done(UART_PORT, ticks) };
	if (tx_ret == ESP_ERR_TIMEOUT)
		ESP_LOGW(TAG, "Function timed out waiting for UART to transmit");
	else if (tx_ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to flush UART buffer (%x)", tx_ret);

	// Return number of bytes written
	return written;
}

int uart_read(char *data_rd, const size_t size, const time_t wait_millis) {
	if (size == 0)
		return 0;

	// Get the amount of ticks to wait
	const TickType_t ticks { wait_millis == 0 ? portMAX_DELAY :
			wait_millis / portTICK_PERIOD_MS };

	// Wait until bytes are read or timeouts
	ESP_LOGD(TAG, "Reading %u bytes from UART", size);
	const int read { uart_read_bytes(UART_PORT,
			reinterpret_cast<uint8_t*>(data_rd), size, ticks) };

	// Log errors or bytes received
	if (read == -1) {
		ESP_LOGE(TAG, "Unable to read data from UART (error)");
	} else if (read != size) {
		ESP_LOGE(TAG, "Unable to read data from UART (expected %u bytes "
				"but got %u)", size, read);
	} else {
		char hex_rd[read * 5];
		ESP_LOGV(TAG, "Received data from UART: [ %s ]", strhex(hex_rd, data_rd));
	}

	// Return the number of bytes read
	return read;
}
