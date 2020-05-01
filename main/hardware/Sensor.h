/*
 * Sensor.h
 *
 *  Created on: Apr 5, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HARDWARE_SENSOR_H_
#define MAIN_HARDWARE_SENSOR_H_

#include <stdint.h>

#include "esp_system.h"
#include "i2c.h"
#include "uart.h"

#include "logger.h"
#include "cJSON.h"

class Sensor {
protected:

	void add_JSON_elem(cJSON *json_root, const char *name, const double value,
			const char *scale) {
		cJSON *base_json_object;
		cJSON_AddItemToObject(json_root, name,
				base_json_object = cJSON_CreateObject());
		cJSON_AddNumberToObject(base_json_object, "value", value);
		cJSON_AddStringToObject(base_json_object, "scale", scale);
	}

public:
	virtual ~Sensor() {
		// Do nothing;
	}

	virtual const char* get_name() {
		// Return the name of the sensor for debugging and logging purposes
		return "Undefined generic sensor";
	}

	virtual esp_err_t ready() {
		// Get the sensor ready a few seconds before it takes a measurement
		// Do nothing by default
		return ESP_OK;
	}

	virtual esp_err_t setup() {
		// Performs initial setup of the sensor, including resetting it
		// Do nothing by default
		return ESP_OK;
	}

	virtual esp_err_t wakeup() {
		// Wake up the sensor some time before the measurement is taken
		// Do nothing by default
		return ESP_OK;
	}

	virtual esp_err_t get_data(cJSON *json_root) {
		// Takes a JSON object and adds sensor data to it
		// Must be overridden
		return ESP_FAIL;
	}

	virtual esp_err_t sleep() {
		// Go into a low-power sleep mode between measurements
		// Do nothing by default
		return ESP_OK;
	}
};

#endif /* MAIN_HARDWARE_SENSOR_H_ */

