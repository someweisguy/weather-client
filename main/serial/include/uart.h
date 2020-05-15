/*
 * uart.h
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERIAL_UART_H_
#define MAIN_SERIAL_UART_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstdint>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "helpers.h"

#define UART_PORT 	UART_NUM_1
#define BUF_SIZE 	1024
#define PIN_NUM_TX 	17 // Adafruit Feather 32 Default
#define PIN_NUM_RX 	16 // Adafruit Feather 32 Default


esp_err_t uart_start();
esp_err_t uart_stop();

int uart_write(const char *data_wr, const size_t size, const time_t wait_millis = 0);
int uart_read(char *data_rd, const size_t size, const time_t wait_millis = 0);

#endif /* MAIN_SERIAL_UART_H_ */
