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
	const char *MICROGRAMS_PER_CUBIC_METER_SYM { "\u03BCg/m\u00B3" },
			*MICRON_SYM { "\u03BCm" }, *COUNT_PER_DECILITER_SYM { "cpdL " };

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

	bool get_data(cJSON *json) override {
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
		const uint16_t received_checksum { static_cast<uint16_t>((buffer[30]
				<< 8) + buffer[31]) };
		if (computed_checksum != received_checksum)
			return false;

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
		build_data(json, "PM 1.0", "PM1", pms_data.pm1_0_std,
				MICROGRAMS_PER_CUBIC_METER_SYM);
		build_data(json, "PM 2.5", "PM2.5", pms_data.pm2_5_std,
				MICROGRAMS_PER_CUBIC_METER_SYM);
		build_data(json, "PM 10.0", "PM10", pms_data.pm10_0_std,
				MICROGRAMS_PER_CUBIC_METER_SYM);

		// Intentionally omit PM atmospheric data because it is not clear what
		//  the difference is between PM standard and PM atmospheric

		// Add particle count data to JSON array object
		build_data(json, "Particles 0.3" "\u03BCm", "PTC0.3", pms_data.part_0_3,
				COUNT_PER_DECILITER_SYM);
		build_data(json, "Particles 0.5" "\u03BCm", "PTC0.5", pms_data.part_0_5,
				COUNT_PER_DECILITER_SYM);
		build_data(json, "Particles 1.0" "\u03BCm", "PTC1", pms_data.part_1_0,
				COUNT_PER_DECILITER_SYM);
		build_data(json, "Particles 2.5" "\u03BCm", "PTC2.5", pms_data.part_2_5,
				COUNT_PER_DECILITER_SYM);
		build_data(json, "Particles 5.0" "\u03BCm", "PTC5", pms_data.part_5_0,
				COUNT_PER_DECILITER_SYM);
		build_data(json, "Particles 10.0" "\u03BCm", "PTC10",
				pms_data.part_10_0, COUNT_PER_DECILITER_SYM);

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
