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
	const char *pm1 { "pm1" }, *pm2_5 { "pm2.5" }, *pm10 { "pm10" },
		*count_per_liter { "count per liter" };

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


	esp_err_t setup() override {
		ESP_LOGV(TAG, "Sending active mode command");
		const char passive_cmd[7] { 0x42, 0x4d, 0xe1, 0x00, 0x01, 0x01, 0x71 };
		return uart_write(passive_cmd, 7);
	}

	esp_err_t wakeup() override {
		ESP_LOGV(TAG, "Sending wake up command");
		const char wakeup_cmd[7] { 0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74 };
		return uart_write(wakeup_cmd, 7);
	}

	esp_err_t get_data(cJSON *json) override {
		uint16_t computed_checksum { 0 }, received_checksum;
		uint8_t retries { 0 };
		char buffer[32];
		do {
			ESP_LOGV(TAG, "Reading data");
			const int read { uart_read(buffer, 32) };
			if (read != 32)
				return read;

			// Compute and check checksum
			ESP_LOGV(TAG, "Computing and comparing checksum");
			for (int i = 0; i < 30; ++i)
				computed_checksum += buffer[i];
			received_checksum = static_cast<uint16_t>((buffer[30] << 8) +
					buffer[31]);
		} while (computed_checksum != received_checksum && ++retries <= 5);
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

		// Add PM data to JSON array object
		add_JSON_elem(json, "PM 1",  pms_data.pm1_0_std, pm1);
		add_JSON_elem(json, "PM 2.5",  pms_data.pm2_5_std, pm2_5);
		add_JSON_elem(json, "PM 10", pms_data.pm10_0_std, pm10);

		// Intentionally omit PM atmospheric data because it is not clear what
		//  the difference is between PM standard and PM atmospheric

		// Add particle count data to JSON array object - multiply by 10 to get
		//  units per Liter
		add_JSON_elem(json, "0.3 microns Particles",
				pms_data.part_0_3 * 10, count_per_liter);
		add_JSON_elem(json, "0.5 microns Particles",
				pms_data.part_0_5 * 10, count_per_liter);
		add_JSON_elem(json, "1 micron Particles",
				pms_data.part_1_0 * 10, count_per_liter);
		add_JSON_elem(json, "2.5 microns Particles",
				pms_data.part_2_5 * 10, count_per_liter);
		add_JSON_elem(json, "5 microns Particles",
				pms_data.part_5_0 * 10.0, count_per_liter);
		add_JSON_elem(json, "10 microns Particles",
				pms_data.part_10_0 * 10, count_per_liter);

		return ESP_OK;
	}

	esp_err_t sleep() override {
		const char sleep_cmd[7] { 0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73 };
		if (uart_write(sleep_cmd, 7) == 7) return ESP_OK;
		else return ESP_FAIL;
	}

};

#endif /* COMPONENTS_SENSORS_PMS5003_H_ */
