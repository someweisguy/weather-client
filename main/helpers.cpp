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
	const char *file_name { "/sdcard/events.log" };
	FILE *fd { fopen(file_name, "a+") };
	if (fd != nullptr && fsize(fd) > 100 * 1024) { // 100 KB
		fclose(fd);
		remove(file_name);
		fd = fopen(file_name, "a+");
	} else if (fd != nullptr) {
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
