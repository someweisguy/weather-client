/*
 * ds3231.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "ds3231.h"
static const char *TAG { "ds3231" };

time_t ds3231_get_time() {
	// Read from the time registers
	ESP_LOGV(TAG, "Reading data from the time registers");
	char data[7];
	if (!i2c_read(I2C_ADDR, DATA_REG_START, data, 7)) {
		ESP_LOGE(TAG, "Unable to read from the time registers");
		return -1;
	}

	// Construct a tm (tm_wday and tm_yday are ignored)
	// see pg.11 https://datasheets.maximintegrated.com/en/ds/DS3231.pdf
	tm t;

	// Get seconds and minutes 0-59
	t.tm_sec = (data[0] >> 4) * 10 | (data[0] & 0xf); // seconds
	t.tm_min = (data[1] >> 4) * 10 | (data[1] & 0xf); // minutes

	// Get hour 0-23 (check if in 12-hour or 24-hour mode)
	t.tm_hour = data[2] & 0xf; 							 	   // get hour 0-9
	t.tm_hour += data[2] & 0x10 ? 10 : 0;					   // add 10 hour bit
	if (data[2] & 0x40) t.tm_hour += data[2] & 0x20 ? 12 : 0;  // add am/pm
	else t.tm_hour += data[2] & 0x20 ? 20 : 0;  			   // add 20 hour bit

	// Get day of month 1-31
	t.tm_mday = (data[4] >> 4) * 10 | (data[4] & 0xf);

	// Get month 0-11
	t.tm_mon = ((data[5] & 0x7) >> 4) * 10 | ((data[5] & 0xf) - 1); // from month 1-12

	// Get year since 1900
	t.tm_year = ((data[6] & 0xf0) * 10 >> 4) | (data[6] & 0xf); // from year 0-99
	t.tm_year += data[5] & 0x80 ? 100 : 0; 		   		  	    // add century bit

	// Set daylight saving time flag
	t.tm_isdst = -1; // information not available

	// Log results in ISO 8601 like format
	ESP_LOGI(TAG, "Got datetime from the DS3231: %d-%02d-%02d %02d:%02d:%02dZ",
			t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
			t.tm_sec);

	// Return the Unix epoch
	return mktime(&t);
}

bool ds3231_set_time() {

	// Get Unix epoch plus 1 second
	timeval tv;
	gettimeofday(&tv, nullptr);
	TickType_t got_time_tick { xTaskGetTickCount() };
	tv.tv_sec += 1;

	// Log the datetime that we are transmitting and how long we will be blocking
	tm *t { gmtime(&tv.tv_sec) };
	int wait_millis = (1e+6 - tv.tv_usec) / 1000;
	const TickType_t send_time_tick { wait_millis / portTICK_PERIOD_MS };
	ESP_LOGI(TAG, "Sending %d-%02d-%02d %02d:%02d:%02dZ to DS3231 in %d millisecond(s)",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min,
			t->tm_sec, wait_millis);

	// Encode the Unix epoch into the data buffer
	// See pg.11 https://datasheets.maximintegrated.com/en/ds/DS3231.pdf
	char data[7];

	// Set seconds and minutes 0-59
	data[0] = (t->tm_sec / 10 << 4) | (t->tm_sec % 10);
	data[1] = (t->tm_min / 10 << 4) | (t->tm_min % 10);

	// Set hours 0-23 (bit 5 is 20 hour, bit 4 is 10 hour, bits 3-0 are 1 hour)
	data[2] = ((t->tm_hour / 10) << 4) | (t->tm_hour % 10);

	// Set weekday 1-7
	data[3] = t->tm_wday + 1; // from weekday 0-6

	// Set day of month 1-31 (bits 5-4 are 10 day, bits 3-0 are 1 day)
	data[4] = (t->tm_mday / 10 << 4) | (t->tm_mday % 10);

	// Set month 1-12 (bit 4 is 10 month, bits 3-0 are 1 month)
	t->tm_mon += 1; // from month 0-11
	data[5] = (t->tm_mon / 10 << 4) | (t->tm_mon % 10);

	// Set year 0-99 (bits 7-4 are 10 year, bits 3-0 are 1 year)
	if (t->tm_year >= 100) {
		data[5] |= 0x80;   // set century bit
		t->tm_year -= 100; // from years since 1900
	}
	data[6] = (t->tm_year / 10 << 4) | (t->tm_year % 10);

	// Wait as close to the start of the second as possible for max accuracy
	vTaskDelayUntil(&got_time_tick, send_time_tick);

	// Write to the time registers
	ESP_LOGV(TAG, "Writing data to the time registers");
	if (!i2c_write(I2C_ADDR, DATA_REG_START, data, 7)) {
		ESP_LOGE(TAG, "Unable to write to the time registers");
		return false;
	}

	// Reset the oscillator stop flag, AKA the lost power flag
	ESP_LOGV(TAG, "Resetting the oscillator stop flag");
	const char osf { 0x8 };
	if (!i2c_write(I2C_ADDR, STATUS_REG, &osf, 1)) {
		ESP_LOGE(TAG, "Unable to reset the oscillator stop flag");
		return false;
	}

	return true;
}

bool ds3231_lost_power() {
	// Read the status register to see if the oscillator stop flag was reset
	unsigned char status;
	ESP_LOGD(TAG, "Checking if the DS3231 lost power");
	if (!i2c_read(I2C_ADDR, STATUS_REG, &status, 1)) {
		ESP_LOGE(TAG, "Unable to communicate with the DS3231");
		return true; // assume power was lost
	}

	// Log results
	if (!(status & 0x80)) {
		ESP_LOGI(TAG, "DS3231 did not lose power");
		return false;
	} else {
		ESP_LOGI(TAG, "DS3231 lost power");
		return true;
	}
}
