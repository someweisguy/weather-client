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

