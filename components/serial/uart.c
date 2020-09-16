#include "uart.h"
#include "driver/uart.h"

#define UART_PORT UART_NUM_1
#define BUF_SIZE 1024
#define PIN_NUM_TX 17 // Adafruit Feather 32 Default
#define PIN_NUM_RX 16 // Adafruit Feather 32 Default

esp_err_t uart_start()
{
	// configure the uart port for the pms5003
	const uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 0,
	};
	uart_param_config(UART_PORT, &uart_config);
	uart_set_pin(UART_PORT, PIN_NUM_TX, PIN_NUM_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	return uart_driver_install(UART_PORT, BUF_SIZE * 2, BUF_SIZE * 2, 10, NULL, 0);
}

esp_err_t uart_stop()
{
	uart_flush(UART_PORT);
	return uart_driver_delete(UART_PORT);
}

esp_err_t uart_write(void *buf, size_t size, time_t wait_ms)
{
	if (size == 0)
		return ESP_OK;

	const int written = uart_write_bytes(UART_PORT, buf, size);

	// get the amount of ticks to wait
	const TickType_t ticks = wait_ms == 0 ? portMAX_DELAY : wait_ms / portTICK_PERIOD_MS;

	esp_err_t err = uart_wait_tx_done(UART_PORT, ticks);
	if (written != size)
		return ESP_ERR_INVALID_SIZE;
	else
		return err;
}

esp_err_t uart_read(void *buf, size_t size, time_t wait_ms)
{
	if (size == 0)
		return ESP_OK;

	// get the amount of ticks to wait
	const TickType_t ticks = wait_ms == 0 ? portMAX_DELAY : wait_ms / portTICK_PERIOD_MS;

	// only read new data
	uart_flush_input(UART_PORT);

	const int read = uart_read_bytes(UART_PORT, buf, size, ticks);

	if (read != size)
		return ESP_ERR_INVALID_SIZE;
	else
		return ESP_OK;
}
