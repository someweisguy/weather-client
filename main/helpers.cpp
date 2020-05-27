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

long fsize(FILE *fd) {
	fpos_t start;
	fgetpos(fd, &start);
	fseek(fd, 0, SEEK_END);
	const long size { ftell(fd) };
	fsetpos(fd, &start);
	return size;
}

esp_err_t get_config_resource(cJSON *&root) {
	// Open the config file and read the resource
	esp_err_t ret { ESP_OK };
	FILE *fd { fopen(CONFIG_FILE, "r") };
	do {
		// Check if the file was opened
		if (fd == nullptr) {
			ret = ESP_ERR_NOT_FOUND;
			return ret;
		}

		// Check if the file is sized correctly
		const long file_size { fsize(fd) };
		if (file_size > CONFIG_MAX_SIZE) {
			ret = ESP_ERR_INVALID_SIZE;
			// allow to continue
		}

		// Check if there is enough memory to read file
		const size_t heap_block { heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) };
		if (file_size + 1 > heap_block) {
			ret = ESP_ERR_NO_MEM;
			break;
		}

		// Read the file into memory
		char* file_str { new char[file_size + 1] };
		if (fread(file_str, 1, file_size, fd) < file_size) {
			ret = ESP_FAIL;
			delete[] file_str;
			break;
		}

		// Parse the string as a JSON object and check that it is valid
		root = cJSON_Parse(file_str);
		delete[] file_str;
		if (cJSON_GetErrorPtr()) {
			ret = ESP_ERR_INVALID_STATE;
			break;
		}
	} while (false);

	if (fclose(fd) == EOF)
		ret = ESP_FAIL;

	return ret;
}

esp_err_t set_config_resource(cJSON *root) {
	// Print the JSON object to a string
	char *json_str { cJSON_Print(root) };
	if (strlen(json_str) > CONFIG_MAX_SIZE) {
		delete[] json_str;
		return ESP_ERR_INVALID_SIZE;
	}

	// Open the config file and write the string
	esp_err_t ret { ESP_OK };
	FILE *fd { fopen(CONFIG_FILE, "w") };
	if (fd != nullptr) {
		if (fputs(json_str, fd) == EOF)
			ret = ESP_FAIL;
		if (fclose(fd) == EOF)
			ret = ESP_FAIL;
	} else ret = ESP_ERR_NOT_FOUND;

	delete[] json_str;
	return ret;
}
