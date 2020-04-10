/*
 * PMS5003.h
 *
 *  Created on: Apr 5, 2020
 *      Author: Mitch
 */

#ifndef COMPONENTS_SENSORS_PMS5003_H_
#define COMPONENTS_SENSORS_PMS5003_H_

#include "Sensor.h"

class PMS5003: public Sensor {
private:

	const char* TAG { "pms5003" };

	struct pms_data_t {
		// Micrograms per cubic meter
		uint16_t pm1_0_std;
		uint16_t pm2_5_std;
		uint16_t pm10_0_std;

		// Nobody know the difference between "std" and "atm" data for this sensor
		uint16_t pm1_0_atm;
		uint16_t pm2_0_atm;
		uint16_t pm10_0_atm;

		// Particles with diameter beyond "X" microns in 0.1L of air
		uint16_t part_0_3;
		uint16_t part_0_5;
		uint16_t part_1_0;
		uint16_t part_2_5;
		uint16_t part_5_0;
		uint16_t part_10_0;
	};

public:

	const char* get_name() override {
		return "PMS5003";
	}

	bool ready() override {
		// Do nothing
		return true;
	}

	bool setup() override {
		verbose(TAG, "Sending passive mode command");
		const uint8_t passive_cmd[7] { 0x42, 0x4d, 0xe1, 0x00, 0x00, 0x01, 0x70 };
		if (uart_write(passive_cmd, 7) == ESP_OK)
			return true;
		else
			return false;
	}

	bool wakeup() override {
		verbose(TAG, "Sending wake up command");
		const uint8_t wakeup_cmd[7] { 0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74 };
		if (uart_write(wakeup_cmd, 7) == ESP_OK)
			return true;
		else
			return false;
	}

	bool get_data(cJSON *json_root) override {
		verbose(TAG, "Sending read in passive mode command");
		const uint8_t read_cmd[7] { 0x42, 0x4d, 0xe2, 0x00, 0x00, 0x01, 0x71 };
		if (uart_write(read_cmd, 7) != ESP_OK)
			return false;

		verbose(TAG, "Reading data");
		uint8_t buffer[32];
		if (uart_read(buffer, 32) != ESP_OK)
			return false;

		// Compute and check checksum
		verbose(TAG, "Computing and comparing checksum");
		uint16_t computed_checksum { 0x42 + 0x4d + (2 * 13 + 2) };
		for (int i = 4; i < 30; ++i) { // skip start word and frame length
			computed_checksum += buffer[i];
		}
		const uint16_t received_checksum { (buffer[30] << 8) + buffer[31] };
		if (computed_checksum != received_checksum)
			return false;

		// Copy the buffer - skip the first 4 and last 4 bytes
		pms_data_t pms_data;
		pms_data.pm1_0_std = (buffer[4] << 8) | buffer[5];
		pms_data.pm2_5_std = (buffer[6] << 8) | buffer[7];
		pms_data.pm10_0_std = (buffer[8] << 8) | buffer[9];
		pms_data.pm1_0_atm = (buffer[10] << 8) | buffer[11];
		pms_data.pm2_0_atm = (buffer[12] << 8) | buffer[13];
		pms_data.pm10_0_atm = (buffer[14] << 8) | buffer[15];
		pms_data.part_0_3 = (buffer[16] << 8) | buffer[17];
		pms_data.part_0_5 = (buffer[18] << 8) | buffer[19];
		pms_data.part_1_0 = (buffer[20] << 8) | buffer[21];
		pms_data.part_2_5 = (buffer[22] << 8) | buffer[23];
		pms_data.part_5_0 = (buffer[24] << 8) | buffer[25];
		pms_data.part_10_0 = (buffer[26] << 8) | buffer[27];

		// Create a PMS5003 JSON object, and add it to root
		cJSON* pms_json;
		cJSON_AddItemToObject(json_root, "pms5003",
				pms_json = cJSON_CreateObject());

		// Add data to PMS5003 JSON object
		cJSON_AddNumberToObject(pms_json, "PM1.0 m3 (std)", pms_data.pm1_0_std);
		cJSON_AddNumberToObject(pms_json, "PM2.5 m3 (std)", pms_data.pm2_5_std);
		cJSON_AddNumberToObject(pms_json, "PM10 m3 (std)", pms_data.pm10_0_std);
		cJSON_AddNumberToObject(pms_json, "PM1.0 m3 (atm)", pms_data.pm1_0_atm);
		cJSON_AddNumberToObject(pms_json, "PM2.5 m3 (atm)", pms_data.pm2_0_atm);
		cJSON_AddNumberToObject(pms_json, "PM10 m3 (atm)", pms_data.pm10_0_atm);
		cJSON_AddNumberToObject(pms_json, "Particles 0.3 um/0.1L", pms_data.part_0_3);
		cJSON_AddNumberToObject(pms_json, "Particles 0.5 um/0.1L", pms_data.part_0_5);
		cJSON_AddNumberToObject(pms_json, "Particles 1.0 um/0.1L", pms_data.part_1_0);
		cJSON_AddNumberToObject(pms_json, "Particles 2.5 um/0.1L", pms_data.part_2_5);
		cJSON_AddNumberToObject(pms_json, "Particles 5.0 um/0.1L", pms_data.part_5_0);
		cJSON_AddNumberToObject(pms_json, "Particles 10.0 um/0.1L", pms_data.part_10_0);

		// Log
		debug(TAG, "PM 1.0: %u (std)", pms_data.pm1_0_std);
		debug(TAG, "PM 2.5: %u (std)", pms_data.pm2_5_std);
		debug(TAG, "PM 10: %u (std)", pms_data.pm10_0_std);
		debug(TAG, "PM 1.0: %u (atm)", pms_data.pm1_0_atm);
		debug(TAG, "PM 2.5: %u (atm)", pms_data.pm2_0_atm);
		debug(TAG, "PM 10: %u (atm)", pms_data.pm10_0_atm);
		debug(TAG, "Particles 0.3 microns: %u", pms_data.part_0_3);
		debug(TAG, "Particles 0.5 microns: %u", pms_data.part_0_5);
		debug(TAG, "Particles 1.0 microns: %u", pms_data.part_1_0);
		debug(TAG, "Particles 2.5 microns: %u", pms_data.part_2_5);
		debug(TAG, "Particles 5.0 microns: %u", pms_data.part_5_0);
		debug(TAG, "Particles 10 microns: %u", pms_data.part_10_0);

		return true;
	}

	bool sleep() override {
		const uint8_t sleep_cmd[7] { 0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73 };
		if (uart_write(sleep_cmd, 7) == ESP_OK)
			return true;
		else
			return false;
	}

};

#endif /* COMPONENTS_SENSORS_PMS5003_H_ */
