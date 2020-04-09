/*
 * uart.h
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#ifndef COMPONENTS_GNDCTRL_UART_GNDCTRL_UART_H_
#define COMPONENTS_GNDCTRL_UART_GNDCTRL_UART_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "logger.h"

// Hardware pin defines
#define PIN_NUM_TX (gpio_num_t) 17 // Adafruit Feather 32 Default
#define PIN_NUM_RX (gpio_num_t) 16 // Adafruit Feather 32 Default

esp_err_t uart_start();
esp_err_t uart_stop();

esp_err_t uart_write(const uint8_t *data_wr, const size_t size);
esp_err_t uart_read(uint8_t *data_rd, const size_t size);

#endif /* COMPONENTS_GNDCTRL_UART_GNDCTRL_UART_H_ */
