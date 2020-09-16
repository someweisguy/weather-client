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

/**
 * Starts the UART bus.
 *
 * @return true on success
 */
bool uart_start();

/**
 * Stops the UART bus.
 *
 * @return true on success
 */
bool uart_stop();

/**
 * Write to the UART bus.
 *
 * @param wr		a pointer to the data to be written
 * @param size		the size in bytes of the data
 * @param wait_ms	the time to wait before the function times out (default 0 or 'no timeout')
 *
 * @return the number of bytes written or -1 on failure
 */
int uart_write(const char *wr, const uint32_t size, const time_t wait_ms = 0);

/**
 * Read from the UART bus.
 *
 * @param rd		a pointer to the destination buffer for the data to be stored
 * @param size		the size in bytes of the data
 * @param wait_ms	the time to wait before the function times out (default 0 or 'no timeout')
 *
 * @return the number of bytes read or -1 on failure
 */
int uart_read(void *rd, const uint32_t size, const time_t wait_ms = 0);

#endif /* MAIN_SERIAL_UART_H_ */
