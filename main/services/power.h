/*
 * power.h
 *
 *  Created on: May 14, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERVICES_POWER_H_
#define MAIN_SERVICES_POWER_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE



#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"

#define FEATHER_LED_PIN 		13
#define BATT_INTERRUPT_PIN   34


void configure_battery_interrupt();

#endif /* MAIN_SERVICES_POWER_H_ */
