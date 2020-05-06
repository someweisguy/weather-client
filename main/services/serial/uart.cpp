/*
 *
 * uart.cpp
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#include "uart.h"
static const char* TAG { "uart" };
static const size_t BUF_SIZE { 1024 };
static const uart_port_t UART_PORT { UART_NUM_1 };


esp_err_t uart_start() {
	uart_config_t uart_config;
	uart_config.baud_rate = 9600;
	uart_config.data_bits = UART_DATA_8_BITS;
	uart_config.parity = UART_PARITY_DISABLE;
	uart_config.stop_bits = UART_STOP_BITS_1;
	uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uart_config.rx_flow_ctrl_thresh = 0;

	verbose(TAG, "Configuring UART port");
	if (uart_param_config(UART_PORT, &uart_config) != ESP_OK)
		return ESP_FAIL;
	if (uart_set_pin(UART_PORT, PIN_NUM_TX, PIN_NUM_RX, UART_PIN_NO_CHANGE,
			UART_PIN_NO_CHANGE) != ESP_OK)
		return ESP_FAIL;

	// Install the driver
	verbose(TAG, "Installing UART driver");
	QueueHandle_t uart_queue;
	return uart_driver_install(UART_PORT, BUF_SIZE * 2, BUF_SIZE * 2, 10,
			&uart_queue, 0);
}

esp_err_t uart_stop() {
	uart_flush(UART_PORT);
	return uart_driver_delete(UART_PORT);
}

esp_err_t uart_write(const uint8_t *data_wr, const size_t size) {
	// Send the data on the UART bus
	const int write { uart_write_bytes(UART_PORT, (char*) data_wr, size) };
	if (write == -1)
		return ESP_FAIL;
	else if (write != size)
		return ESP_ERR_INVALID_SIZE;
	return uart_wait_tx_done(UART_PORT, 5000 / portTICK_PERIOD_MS);
}

esp_err_t uart_read(uint8_t *data_rd, const size_t size) {
	// Wait 1 second to read data on the UART bus
	const int read { uart_read_bytes(UART_PORT, data_rd, size,
			5000 / portTICK_PERIOD_MS) };
	if (read == -1)
		return ESP_FAIL;
	else if (read != size)
		return ESP_ERR_INVALID_SIZE;
	else
		return ESP_OK;
}
