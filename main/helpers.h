/*
 * bsp.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HELPERS_H_
#define MAIN_HELPERS_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "driver/adc.h"
#include "driver/gpio.h"

#include "wlan.h"
#include "sdcard.h"

/**
 * A vprintf like function to be used for logging in the ESP Logging Library.
 * Set this function as the argument to esp_log_set_vprintf and the ESP32
 * logging macros will use this function to print to log. This function prints
 * normally to console, and it also prints to the '/events.log' file in the SD
 * card.
 *
 * @param format	the log message as a format string
 * @param arg		a va_list with the format string arguments
 *
 * @return
 * 		- (-1) a writing error occurred
 * 		- others (>=0) the total number of characters written
 */
int vlogf(const char *format, va_list arg);

/**
 * Sets the system time to the corresponding Unix epoch.
 *
 * @param epoch	 the Unix epoch in seconds since 1 January 1970
 */
void set_system_time(const time_t epoch);

/**
 * Gets the system time as the Unix epoch. Sets a timeval struct if it was
 * passed.
 *
 * @param tv	an empty timeval to be set to get precise time.
 *
 * @return the time in seconds since 1 January 1970
 */
time_t get_system_time(struct timeval *tv = nullptr);

int32_t get_line_length(FILE *f);

/**
 * Create a hexadecimal representation of the source string and store it in the
 * destination string. The hexadecimal string is separated into 8 bit segments
 * with a space. For example, the source string 'Hello world!' returns
 * '48 65 6C 6C 6F 20 77 6F 72 6C 64'.
 *
 * @note you should allocate strlen(source) * 5 bytes for the destination string
 * (this includes the null terminator since there is no space after the last
 * hex value)
 *
 * @param destination	the destination string
 * @param source		the source string
 *
 * @return 				a pointer to the destination string
 */
char *strhex(char *destination, const char *source);

/**
 * Create a hexadecimal representation of the source string of size num and
 * store it in the destination string. The hexadecimal string is separated into
 * 8 bit segments with a space. For example, the source string 'Hello world!'
 * returns '48 65 6C 6C 6F 20 77 6F 72 6C 64'.
 *
 * @note you should allocate strlen(source) * 5 bytes for the destination string
 * (this includes the null terminator since there is no space after the last
 * hex value)
 *
 * @param destination	the destination string
 * @param source		the source string
 * @param num			maximum number of characters to be copied from source
 *
 * @return 				a pointer to the destination string
 */
char *strnhex(char *destination, const char *source, size_t num);

/**
 * Returns the file size in bytes of a file using its file descriptor.
 *
 * @param fd		the file descriptor of a file
 *
 * @return 			the size of the file in bytes
 */
int fsize(FILE *fd);

/**
 *
 */
time_t get_window_wait_ms(const int modifier_ms);

#endif /* MAIN_HELPERS_H_ */
