/*
 * bsp.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */


#include "helpers.h"

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
	fpos_t start;
	fgetpos(fd, &start);
	fseek(fd, 0, SEEK_END);
	const long size { ftell(fd) };
	fsetpos(fd, &start);
	return size;
}
