/*
 * Sensor.h
 *
 *  Created on: Apr 5, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HARDWARE_SENSOR_H_
#define MAIN_HARDWARE_SENSOR_H_

#include <stdint.h>

#include "i2c.h"
#include "uart.h"

#include "logger.h"
#include "cJSON.h"

class Sensor {
protected:
	cJSON *build_data(const char *name, const char *abbreviated_name,
			double val, const char* unit) {
		cJSON *data_root;
		data_root = cJSON_CreateObject();
		cJSON_AddStringToObject(data_root, "name", name);
		if (strcmp(abbreviated_name, "") != 0)
			cJSON_AddStringToObject(data_root, "abbr", abbreviated_name);
		cJSON_AddNumberToObject(data_root, "val", val);
		if (strcmp(unit, "") != 0)
			cJSON_AddStringToObject(data_root, "unit", unit);
		return data_root;
	}

public:
	virtual ~Sensor() {
		// Do nothing;
	}

	virtual const char* get_name() {
		// Return the name of the sensor for debugging and logging purposes
		return "Undefined generic sensor";
	}

	virtual bool ready() {
		// Get the sensor ready a few seconds before it takes a measurement
		// Do nothing by default
		return true;
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
		return false;
	}

	virtual bool sleep() {
		// Go into a low-power sleep mode between measurements
		// Do nothing by default
		return true;
	}
};

#endif /* MAIN_HARDWARE_SENSOR_H_ */

