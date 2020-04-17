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

	const char *TAG { "pms5003" };

	struct pms_data_t {
		// Micrograms per cubic meter
		uint16_t pm1_0_std;
		uint16_t pm2_5_std;
		uint16_t pm10_0_std;

		// Nobody know the difference between standard and atmospheric data for this sensor
		uint16_t pm1_0_atm;
		uint16_t pm2_5_atm;
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

	esp_err_t ready() override {
		// Do nothing
		return ESP_OK;
	}

	esp_err_t setup() override {
		verbose(TAG, "Sending active mode command");
		const uint8_t passive_cmd[7] { 0x42, 0x4d, 0xe1, 0x00, 0x01, 0x01, 0x71 };
		return uart_write(passive_cmd, 7);
	}

	esp_err_t wakeup() override {
		verbose(TAG, "Sending wake up command");
		const uint8_t wakeup_cmd[7] { 0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74 };
		return uart_write(wakeup_cmd, 7);
	}

	esp_err_t get_data(cJSON *json) override {
		verbose(TAG, "Reading data");
		uint8_t buffer[32];
		const esp_err_t read_ret { uart_read(buffer, 32) };
		if (read_ret != ESP_OK)
			return read_ret;

		// Compute and check checksum
		verbose(TAG, "Computing and comparing checksum");
		uint16_t computed_checksum { 0 };
		for (int i = 0; i < 30; ++i)
			computed_checksum += buffer[i];
		const uint16_t received_checksum { static_cast<uint16_t>((buffer[30]
				<< 8) + buffer[31]) };
		if (computed_checksum != received_checksum)
			return ESP_ERR_INVALID_CRC;

		// Copy the buffer - skip the first 4 and last 4 bytes
		pms_data_t pms_data;
		pms_data.pm1_0_std = (buffer[4] << 8) | buffer[5];
		pms_data.pm2_5_std = (buffer[6] << 8) | buffer[7];
		pms_data.pm10_0_std = (buffer[8] << 8) | buffer[9];
		pms_data.pm1_0_atm = (buffer[10] << 8) | buffer[11];
		pms_data.pm2_5_atm = (buffer[12] << 8) | buffer[13];
		pms_data.pm10_0_atm = (buffer[14] << 8) | buffer[15];
		pms_data.part_0_3 = (buffer[16] << 8) | buffer[17];
		pms_data.part_0_5 = (buffer[18] << 8) | buffer[19];
		pms_data.part_1_0 = (buffer[20] << 8) | buffer[21];
		pms_data.part_2_5 = (buffer[22] << 8) | buffer[23];
		pms_data.part_5_0 = (buffer[24] << 8) | buffer[25];
		pms_data.part_10_0 = (buffer[26] << 8) | buffer[27];

		const char *MICROGRAMS_PER_CUBIC_METER_SYM { "\u03BCg/m\u00B3" };

		// Add PM data to JSON array object
		add_JSON_elem(json, "PM 1.0",  pms_data.pm1_0_std, "",
				MICROGRAMS_PER_CUBIC_METER_SYM);
		add_JSON_elem(json, "PM 2.5",  pms_data.pm2_5_std, "",
				MICROGRAMS_PER_CUBIC_METER_SYM);
		add_JSON_elem(json, "PM 10.0", pms_data.pm10_0_std, "",
				MICROGRAMS_PER_CUBIC_METER_SYM);

		// Intentionally omit PM atmospheric data because it is not clear what
		//  the difference is between PM standard and PM atmospheric

		// Add particle count data to JSON array object - multiply by 10 to get
		//  units per Liter
		add_JSON_elem(json, ">0.3 micron Dia. Particles",
				pms_data.part_0_3 * 10.0, "", "U/L");
		add_JSON_elem(json, ">0.5 micron Dia. Particles",
				pms_data.part_0_5 * 10.0, "", "U/L");
		add_JSON_elem(json, ">1.0 micron Dia. Particles",
				pms_data.part_1_0 * 10.0, "", "U/L");
		add_JSON_elem(json, ">2.5 micron Dia. Particles",
				pms_data.part_2_5 * 10.0, "", "U/L");
		add_JSON_elem(json, ">5.0 micron Dia. Particles",
				pms_data.part_5_0 * 10.0, "", "U/L");
		add_JSON_elem(json, ">10.0 micron Dia. Particles",
				pms_data.part_10_0 * 10.0, "", "U/L");

		return ESP_OK;
	}

	esp_err_t sleep() override {
		const uint8_t sleep_cmd[7] { 0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73 };
		return uart_write(sleep_cmd, 7);
	}

};

#endif /* COMPONENTS_SENSORS_PMS5003_H_ */
