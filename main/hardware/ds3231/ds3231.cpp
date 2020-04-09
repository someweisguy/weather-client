/*
 * ds3231.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "ds3231.h"
static const char* TAG { "ds3231" };
static const uint8_t I2C_ADDR { 0x68 }, STATUS_REG { 0x0f },
	DATA_REG_START { 0x00 }, TEMP_REG_START { 0x11 };

esp_err_t ds3231_lost_power(bool &lost_power) {
	// Get the "lost power" bit from the status register
	uint8_t status;
	if (i2c_read(I2C_ADDR, STATUS_REG, &status, 1) != ESP_OK)
		return ESP_FAIL;
	lost_power = status >> 7;
	return ESP_OK;
}

esp_err_t ds3231_get_time(time_t &unix_time) {
	// Read the time data from the DS3231
	uint8_t data[8];
	if (i2c_read(I2C_ADDR, DATA_REG_START, data, 7) != ESP_OK)
		return ESP_FAIL;

	// Decode the time data
	// See pg. 11: https://datasheets.maximintegrated.com/en/ds/DS3231.pdf
	tm time;
	time.tm_sec = (data[0] >> 4) * 10 + (data[0] & 0x0F); // seconds
	time.tm_min = (data[1] >> 4) * 10 + (data[1] & 0x0F); // minutes
	time.tm_hour = (data[2] >> 4) * 10 + (data[2] & 0x0F); // hours
	time.tm_wday = data[3] - 1; // weekday
	time.tm_mday = (data[4] >> 4) * 10 + ((data[4] & 0x0F) % 10); // date
	time.tm_mon = ((0x7F & data[5]) >> 4) * 10 + ((data[5] & 0x0F) % 10) - 1; // month
	time.tm_year = (data[6] >> 4) * 10 + (data[6] & 0x0F) + 100;
	verbose(TAG, "Got time: %02i/%02i/%i %02i:%02i:%02i ", time.tm_mon, time.tm_mday,
			time.tm_year + 1900, time.tm_hour, time.tm_min, time.tm_sec);

	unix_time = mktime(&time);
	return ESP_OK;
}

esp_err_t ds3231_set_time(const time_t unix_time) {

	// Convert the time to the format the DS3231 uses
	// See pg. 11: https://datasheets.maximintegrated.com/en/ds/DS3231.pdf
	uint8_t data[8];
	tm *time = localtime(&unix_time);
	data[0] = ((time->tm_sec / 10) << 4) | (time->tm_sec % 10); // seconds
	data[1] = ((time->tm_min / 10) << 4) | (time->tm_min % 10); // minutes
	data[2] = ((time->tm_hour / 10) << 4) | (time->tm_hour % 10); // hours
	data[3] = time->tm_wday + 1; // weekday
	data[4] = ((time->tm_mday / 10) << 4) | (time->tm_mday % 10); // date
	data[5] = (0x80 | (((time->tm_mon + 1) / 10) << 4) | ((time->tm_mon + 1) % 10)); // month
	data[6] = (((time->tm_year - 100) / 10) << 4 | ((time->tm_year - 100) % 10)); // year

	// Send the time to the DS3231
	verbose(TAG, "Sending time: %02i/%02i/%i %02i:%02i:%02i ", time->tm_mon,
			time->tm_mday, time->tm_year + 1900, time->tm_hour, time->tm_min,
			time->tm_sec);
	if (i2c_write(I2C_ADDR, 0x0, data, 7) != ESP_OK)
		return ESP_FAIL;

	// Reset the lost power flag
	const uint8_t STATUS_BYTE { 0x08 };
	if (i2c_write(I2C_ADDR, STATUS_REG, &STATUS_BYTE, 1) != ESP_OK)
		return ESP_FAIL;

	return ESP_OK;
}

esp_err_t ds3231_get_temperature_celsius(float &temperature_C) {
	verbose(TAG, "Getting temperature");
	int16_t buffer;
	if (i2c_read(I2C_ADDR, TEMP_REG_START, &buffer, 2) != ESP_OK)
		return ESP_FAIL;

	// Bit-shift right 6 bits keeping the sign, then multiply by 0.25 Celsius
	temperature_C = (buffer / 64) * 0.25;
	return ESP_OK;
}
