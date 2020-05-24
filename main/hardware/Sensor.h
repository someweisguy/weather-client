/*
 * Sensor.h
 *
 *  Created on: Apr 5, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HARDWARE_SENSOR_H_
#define MAIN_HARDWARE_SENSOR_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstdint>
#include "esp_system.h"
#include "esp_log.h"
#include "i2c.h"
#include "uart.h"
#include "cJSON.h"
#include "helpers.h"

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

	virtual bool setup() {
		// Performs initial setup of the sensor, including resetting it
		// Do nothing by default
		return true;
	}

	virtual bool wakeup() {
		// Wake up the sensor some time before the measurement is taken
		// Do nothing by default
		return true;
	}

	virtual bool get_data(cJSON *json_root) {
		// Takes a JSON object and adds sensor data to it
		// Must be overridden
		ESP_LOGW("sensor", "Undefined generic sensor has no data");
		return true;
	}

	virtual bool sleep() {
		// Go into a low-power sleep mode between measurements
		// Do nothing by default
		return true;
	}
};

#endif /* MAIN_HARDWARE_SENSOR_H_ */
