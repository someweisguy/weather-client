/*
 * bsp.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */


#include "helpers.h"

int vlogf(const char *format, va_list arg) {
	// Get an appropriate sized buffer for the message
	const int len { vsnprintf(nullptr, 0, format, arg) };
	char message[len + 1];
	vsprintf(message, format, arg);

	// Open the file and delete it if it gets too big
	FILE *fd { fopen(LOG_FILE, "a+") };
	if (fd != nullptr) {
		// Delete the log file if it gets too big
		if (fsize(fd) > 100 * 1024) {
			// TODO: replace with freopen(LOG_FILE_PATH, "w", fd) ?
			fclose(fd);
			remove(LOG_FILE);
			fd = fopen(LOG_FILE, "a+");
		}

		// Print everything to file except ASCII color codes
		bool in_esc { false };
		for (int i = 0; i < len; ++i) {
			if (in_esc) {
				if (message[i] == 'm') in_esc = false;
				else continue;
			} else {
				if (message[i] == '\033') in_esc = true;
				else fputc(message[i], fd);
			}
		}
		fclose(fd);
	}

	// Print to stdout
	return fputs(message, stdout);
}

void set_system_time(const time_t epoch, const char* timezone_str) {
	timeval tv;
	tv.tv_sec = epoch;
	tv.tv_usec = 0;
	settimeofday(&tv, nullptr);

	// Set the time zone
	if (timezone_str != nullptr) {
		// See: https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
		setenv("TZ", timezone_str, 1);
		tzset();
	}
}

time_t get_system_time(struct timeval *tv) {
	gettimeofday(tv, nullptr);
	if (tv != nullptr)
		return tv->tv_sec;
	else
		return time(nullptr);
}

int64_t set_window_wait_timer(esp_timer_handle_t &timer, const int ms) {
	// Calculate next wake time
	timeval tv;
	int64_t window_delta_us;
	get_system_time(&tv);
	vTaskSuspendAll(); // critical section
	window_delta_us = (300 - tv.tv_sec % 300) * 1e+6 - tv.tv_usec + ms * 1000;
	if (window_delta_us < 0) // skip window
		window_delta_us += 5 * 60 * 1e+6; // 5 minutes
	esp_timer_start_once(timer, window_delta_us);
	xTaskResumeAll(); // end critical section

	return window_delta_us / 1000; // millis
}

char *strhex(char *destination, const char *source) {
	const unsigned int len { strlen(source) };
	return strnhex(destination, source, len);
}

char *strnhex(char *destination, const char *source, size_t num) {
	for (int i = 0, j = 0; i < num; ++i, j += 5) {
		sprintf(destination + j, "0x%02X", source[i]);
		if (i != num - 1) destination[j + 4] = ' ';
	}
	return destination;
}

char *ms2str(char* destination, int64_t millis) {
	if (millis < 0) millis = -millis;
	const int mins { static_cast<int>(millis / (60 * 1000)) };
	millis %= 60 * 1000;
	const int secs { static_cast<int>(millis / 1000) };
	millis %= 1000;
	sprintf(destination, "%02i:%02i.%03lli", mins, secs, millis);
	return destination;
}

int fsize(FILE *fd) {
	fpos_t start;
	fgetpos(fd, &start);
	fseek(fd, 0, SEEK_END);
	const long size { ftell(fd) };
	fsetpos(fd, &start);
	return size;
}

esp_err_t get_resource(cJSON *&root) {
	esp_err_t ret;
	FILE *fd { fopen(CONFIG_FILE, "r") };
	if (fd != nullptr) {
		const long size { fsize(fd) };
		if (size > CONFIG_MAX_SIZE) {
			ret = ESP_ERR_INVALID_SIZE;
		} else {
			char file_str[size + 1];
			fread(file_str, 1, size, fd);
			root = cJSON_Parse(file_str);
			if (cJSON_GetErrorPtr())
				ret = ESP_ERR_INVALID_STATE;
			else
				ret = ESP_OK;
		}
	} else {
		return ESP_ERR_NOT_FOUND;
	}
	fclose(fd);
	return ret;
}
