/*
 * BME280.h
 *
 *  Created on: Apr 5, 2020
 *      Author: Mitch
 */

#ifndef COMPONENTS_SENSORS_BME280_H_
#define COMPONENTS_SENSORS_BME280_H_

#include <cmath>

#include "Sensor.h"

class BME280: public Sensor {
private:
	const char *TAG { "bme280" };
	const char *celsius { "celsius" }, *pascals { "pascals" },
			*relative_humidity_scale { "relative humidity" };

	const uint8_t I2C_ADDRESS 	{ 0x76 },
			REG_CHIP_ID 		{ 0xd0 },
			REG_RESET 			{ 0xe0 },
			REG_CONTROLHUMID 	{ 0xf2 },
			REG_STATUS 			{ 0xf3 },
			REG_CTRL_MEAS 		{ 0xf4 },
			REG_CONFIG 			{ 0xf5 },
			REG_DATA_START 		{ 0xf7 },
			REG_TRIM_T1_TO_H1 	{ 0x88 },
			REG_TRIM_H2_TO_H5 	{ 0xe1 };

	int32_t t_fine;
	struct comp_val_t {
		uint16_t t1;
		int16_t t2;
		int16_t t3;

		uint16_t p1;
		int16_t p2;
		int16_t p3;
		int16_t p4;
		int16_t p5;
		int16_t p6;
		int16_t p7;
		int16_t p8;
		int16_t p9;

		uint8_t h1;
		int16_t h2;
		uint8_t h3;
		int16_t h4;
		int16_t h5;
		int8_t h6;
	} dig;

	int32_t calculate_t_fine(const int32_t adc_T) {
		const int32_t var1 = ((((adc_T >> 3) - ((int32_t) dig.t1 << 1)))
				* ((int32_t) dig.t2)) >> 11;
		const int32_t var2 = (((((adc_T >> 4) - ((int32_t) dig.t1))
				* ((adc_T >> 4) - ((int32_t) dig.t1))) >> 12)
				* ((int32_t) dig.t3)) >> 14;
		const int32_t T = var1 + var2;
		return T;
	}

	int32_t compensate_temperature() {
		// Return temperature in 1/100ths of a degree Celsius
		return (t_fine * 5 + 128) >> 8;
	}

	uint32_t compensate_pressure(const int32_t adc_P) {
		// Return pressure in Pascals * 256
		int64_t var1, var2, P;
		var1 = ((int64_t) t_fine) - 128000;
		var2 = var1 * var1 * (int64_t) dig.p6;
		var2 = var2 + ((var1 * (int64_t) dig.p5) << 17);
		var2 = var2 + (((int64_t) dig.p4) << 35);
		var1 = ((var1 * var1 * (int64_t) dig.p3) >> 8)
				+ ((var1 * (int64_t) dig.p2) << 12);
		var1 = (((((int64_t) 1) << 47) + var1)) * ((int64_t) dig.p1) >> 33;
		if (var1 == 0)
			return 0; // avoid divide by zero
		P = 1048576 - adc_P;
		P = (((P << 31) - var2) * 3125) / var1;
		var1 = (((int64_t) dig.p9) * (P >> 13) * (P >> 13)) >> 25;
		var2 = (((int64_t) dig.p8) * P) >> 19;
		P = ((P + var1 + var2) >> 8) + (((int64_t) dig.p7) << 4);

		return P;
	}

	uint32_t compensate_humidity(const int32_t adc_H) {
		// Return relative humidity * 1024
		int32_t v_x1_u32r;
		v_x1_u32r = (t_fine - ((int32_t) 76800));
		v_x1_u32r = (((((adc_H << 14) - (((int32_t) dig.h4) << 20)
				- (((int32_t) dig.h5) * v_x1_u32r)) + ((int32_t) 16384)) >> 15)
				* (((((((v_x1_u32r * ((int32_t) dig.h6)) >> 10)
						* (((v_x1_u32r * ((int32_t) dig.h3)) >> 11)
								+ ((int32_t) 32768))) >> 10)
						+ ((int32_t) 2097152)) * ((int32_t) dig.h2) + 8192)
						>> 14));
		v_x1_u32r = (v_x1_u32r
				- (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7)
						* ((int32_t) dig.h1)) >> 4));
		v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
		v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
		const uint32_t H = (uint32_t) (v_x1_u32r >> 12);

		return H;
	}

public:
	const char* get_name() override {
		return "BME280";
	}

	bool setup() override {
		// Soft reset the BME280
		ESP_LOGD(TAG, "Sending soft reset command");
		const char reset_cmd { 0xb6 };
		if (!i2c_write(I2C_ADDRESS, REG_RESET, &reset_cmd, 1)) {
			ESP_LOGE(TAG, "Unable to send soft reset command");
			return false;
		}

		// Wait for the reset to take effect
		ESP_LOGD(TAG, "Waiting for response to soft reset");
		char status;
		do {
			vTaskDelay(10 / portTICK_PERIOD_MS);
			if (!i2c_read(I2C_ADDRESS, REG_STATUS, &status, 1)) {
				ESP_LOGE(TAG, "Unable to read ready bit");
				return false;
			}
		} while (status & 0x01);

		// Ensure that chip ID is correct
		ESP_LOGD(TAG, "Getting chip ID");
		char id;
		if (!i2c_read(I2C_ADDRESS, REG_CHIP_ID, &id, 1)) {
			ESP_LOGE(TAG, "Unable to read chip ID");
			return false;
		} else if (id != 0x60) {
			ESP_LOGE(TAG, "Got wrong chip ID (expected 0x60, got 0x%02X)", id);
			return false;
		}

		// Set the sensor to sleep mode, otherwise settings will be ignored
		ESP_LOGD(TAG, "Sending sleep command");
		const char sleep_cmd { 0x00 };
		if (!i2c_write(I2C_ADDRESS, REG_CTRL_MEAS, &sleep_cmd, 1)) {
			ESP_LOGE(TAG, "Unable to send sleep command during setup");
			return false;
		}

		// Set filtering x16 and set the standby in normal mode to 20ms
		ESP_LOGD(TAG, "Setting filtering x16 and standby 20ms");
		const char filter_standby { 0xf0 };
		if (!i2c_write(I2C_ADDRESS, REG_CONFIG, &filter_standby, 1)) {
			ESP_LOGE(TAG, "Unable to set filtering x16 and standby 20ms");
			return false;
		}

		// Write the sample rate to the Humidity control register
		ESP_LOGD(TAG, "Setting humidity sampling x16");
		const char hum_sample { 0x05 };
		if (!i2c_write(I2C_ADDRESS, REG_CONTROLHUMID, &hum_sample, 1)) {
			ESP_LOGE(TAG, "Unable to set humidity sampling x16");
			return false;
		}

		// Set temperature and pressure sampling to x16 and sleep mode
		ESP_LOGD(TAG, "Setting temperature and pressure sampling x16 and normal mode");
		const char tpm_setting { 0xb7 };
		if (!i2c_write(I2C_ADDRESS, REG_CTRL_MEAS, &tpm_setting, 1)) {
			ESP_LOGE(TAG, "Unable to set temperature and pressuring sampling "
					"x16 and normal mode");
			return false;
		}

		// Get compensation "dig" values
		char dig_buf[32];
		ESP_LOGV(TAG, "Getting data trim values");
		if (!i2c_read(I2C_ADDRESS, REG_TRIM_T1_TO_H1, dig_buf, 25)
				|| !i2c_read(I2C_ADDRESS, REG_TRIM_H2_TO_H5, dig_buf + 25, 7)) {
			ESP_LOGE(TAG, "Unable to read data trim values");
			return false;
		}
		// Bulk copy T1 to H1
		memcpy(&dig, dig_buf, 25);
		// Copy H2 to H6
		dig.h2 = (dig_buf[26] << 8) | dig_buf[25];
		dig.h3 = dig_buf[27];
		dig.h4 = (dig_buf[28] << 4) | (dig_buf[29] & 0x0f);
		dig.h5 = (dig_buf[30] << 4) | (dig_buf[29] >> 4);
		dig.h6 = dig_buf[31];

		return true;
	}

	bool wakeup() override {
		// Set temperature and pressure sampling to x16 and normal mode
		ESP_LOGD(TAG, "Sending wakeup command");
		const char tpm_setting { 0xb7 };
		if (!i2c_write(I2C_ADDRESS, REG_CTRL_MEAS, &tpm_setting, 1)) {
			ESP_LOGE(TAG, "Unable send wakeup command");
			return false;
		}

		return true;
	}

	bool get_data(cJSON *json) override {
		// Store all the ADC values from the BME280 into a buffer
		char buf[8];
		if (!i2c_read(I2C_ADDRESS, REG_DATA_START, buf, 8)) {
			ESP_LOGE(TAG, "Unable to read ADC values from device");
			return false;
		}

		// Log the hex received
		char hex_buf[8*5];
		strnhex(hex_buf, buf, 8);
		ESP_LOGD(TAG, "Got hex: %s", hex_buf);

		// Extract the temperature ADC values from the buffer
		int32_t adc_T = buf[3];
		for (int i = 4; i < 6; ++i) {
			adc_T <<= 8;
			adc_T |= buf[i];
		}

		// Calculate the temperature in Celsius
		if (adc_T != 0x800000) {
			adc_T >>= 4;
			t_fine = calculate_t_fine(adc_T);
			const double temperature_C { compensate_temperature() / 100.0 };
			ESP_LOGD(TAG, "Got temperature: %.2f C", temperature_C);
			add_JSON_elem(json, "Temperature", temperature_C, celsius);
		} else {
			ESP_LOGE(TAG, "Unable to get data (invalid state)");
			return false; // temperature must be turned on
		}

		// Extract the pressure ADC values from the buffer
		int32_t adc_P = buf[0];
		for (int i = 1; i < 3; ++i) {
			adc_P <<= 8;
			adc_P |= buf[i];
		}

		// Calculate pressure in Pascals
		if (adc_P != 0x800000) {
			adc_P >>= 4;
			const uint64_t pressure_Pa { compensate_pressure(adc_P) / 256 };
			ESP_LOGD(TAG, "Got pressure: %llu Pa", pressure_Pa);
			add_JSON_elem(json, "Barometric Pressure", pressure_Pa, pascals);
		} else {
			ESP_LOGW(TAG, "Unable to get pressure data (invalid state)");
		}

		// Extract the pressure ADC values from the buffer
		int32_t adc_H = (buf[6] << 8) | buf[7];

		// Calculate relative humidity
		if (adc_H != 0x8000) {
			const double relative_humidity { compensate_humidity(adc_H) / 1024.0 };
			ESP_LOGD(TAG, "Got humidity: %.2f %%RH", relative_humidity);
			add_JSON_elem(json, "Relative Humidity", relative_humidity,
					relative_humidity_scale);
		} else {
			ESP_LOGW(TAG, "Unable to get relative humidity data (invalid state)");
		}

		return true;
	}

	bool sleep() override {
		// Set temperature and pressure sampling to x16 and sleep mode
		ESP_LOGD(TAG, "Sending sleep command");
		const char tpm_setting { 0xb4 };
		if (!i2c_write(I2C_ADDRESS, REG_CTRL_MEAS, &tpm_setting, 1)) {
			ESP_LOGE(TAG, "Unable to send sleep command");
			return false;
		}

		return true;
	}

};

#endif /* COMPONENTS_SENSORS_BME280_H_ */
