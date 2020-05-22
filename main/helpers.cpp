/*
 * bsp.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */


#include "helpers.h"
static const char *TAG { "helpers" };

const char* esp_reset_to_name(esp_reset_reason_t code) {
	switch (code) {
	case ESP_RST_POWERON: return "power-on event";
	case ESP_RST_EXT: return "external pin reset";
	case ESP_RST_SW: return "software API reset or abort";
	case ESP_RST_PANIC: return "exception/panic reset";
	case ESP_RST_INT_WDT: return "interrupt watchdog reset";
	case ESP_RST_TASK_WDT: return "task watchdog reset";
	case ESP_RST_WDT: return "other watchdog reset";
	case ESP_RST_DEEPSLEEP: return "exiting deep sleep reset";
	case ESP_RST_BROWNOUT: return "brownout reset";
	case ESP_RST_SDIO: return "reset over SDIO";
	case ESP_RST_UNKNOWN: default: return "unknown reset";
	}
}

int vlogf(const char *format, va_list arg) {
	// Get an appropriate sized buffer for the message
	const int len { vsnprintf(nullptr, 0, format, arg) };
	char message[len + 1];
	vsprintf(message, format, arg);

	const char *file_name { "/sdcard/events.log" };
	FILE *fd { sdcard_open(file_name, "a+") };

	// Delete the file if it gets too big
	if (fd != nullptr && fsize(fd) > 100 * 1024) { // 100KB
		fclose(fd);
		remove(file_name);
		fd = fopen(file_name, "a+");
	}

	if (fd != nullptr) {
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
		sdcard_close(fd);
	}

	// Print to stdout
	return fputs(message, stdout);
}

char *strip(char *s) {
    char *p2 = s;
    while (*s != '\0') {
        if (*s != '\t' && *s != '\n')
            *p2++ = *s++;
        else
            ++s;
    }
    *p2 = '\0';
    return s;
}

void set_system_time(const time_t epoch) {
	timeval tv;
	tv.tv_sec = epoch;
	tv.tv_usec = 0;
	settimeofday(&tv, nullptr);
}

time_t get_system_time(struct timeval *tv) {
	gettimeofday(tv, nullptr);
	if (tv != nullptr)
		return tv->tv_sec;
	else
		return time(nullptr);
}

int32_t get_line_length(FILE *f) {
	if (f == nullptr)
		return -1;

	fpos_t start_pos;
	fgetpos(f, &start_pos);
	int c, count { 0 };

	while (true) {
		c = fgetc(f);
		if (c == EOF || c == '\n')
			break;
		++count;
	}
	fsetpos(f, &start_pos);
	return count;
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

int fsize(FILE *fd) {
	struct stat st;
	const int status { fstat(reinterpret_cast<int>(fd), &st) };
	if (status == -1) return -1;
	else return st.st_size;
}

time_t get_wait_ms(const int modifier_ms) {
	// Calculate next wake time
	timeval tv;
	const time_t epoch = get_system_time(&tv);
	// FIXME: window_delta_ms should not be negative
	time_t window_delta_ms = (300 - tv.tv_sec % 300) * 1000 - (tv.tv_usec
			/ 1000) + modifier_ms;
	if (window_delta_ms < 0) {
		ESP_LOGD(TAG, "Skipping next measurement window");
		window_delta_ms += 300000; // 5 minutes
	}
	ESP_LOGD(TAG, "Next window is in %ld ms", window_delta_ms);

	// Log results and wait
	const time_t window_epoch = epoch + window_delta_ms / 1000;
	tm *window_tm = localtime(&window_epoch);
	ESP_LOGI(TAG, "Waiting until %02d:%02d:%02dZ", window_tm->tm_hour,
				window_tm->tm_min, window_tm->tm_sec);
	return window_delta_ms;
}

void synchronize_system_time_task(void *args) {
	while (true) {
		bool time_is_synchronized { false };
		vTaskDelay(604800000 / portTICK_PERIOD_MS); // 1 week
		do {
			if (wlan_connected())
				time_is_synchronized = sntp_synchronize_system_time();
			else {
				ESP_LOGW(TAG, "Unable to synchronize system time (not connected)");
				vTaskDelay(60000 / portTICK_PERIOD_MS);
			}
		} while (!time_is_synchronized);
	}
}
