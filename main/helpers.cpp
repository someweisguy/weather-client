/*
 * bsp.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "helpers.h"

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
	FILE *fd { fopen(file_name, "a+") };

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
		fclose(fd);
	}

	// Print to stdout
	return fputs(message, stdout);
}

void strip(char *s) {
    char *p2 = s;
    while (*s != '\0') {
        if (*s != '\t' && *s != '\n')
            *p2++ = *s++;
        else
            ++s;
    }
    *p2 = '\0';
}

void set_system_time(const time_t epoch) {
	timeval tv;
	tv.tv_sec = epoch;
	tv.tv_usec = 0;
	settimeofday(&tv, nullptr);
}

time_t get_system_time() {
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