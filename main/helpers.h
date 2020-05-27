/*
 * helpers.h
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HELPERS_H_
#define MAIN_HELPERS_H_

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_event.h"
#include "cJSON.h"

#define MOUNT_POINT     CONFIG_RES_SDCARD_MOUNT_POINT
#define CONFIG_FILE     MOUNT_POINT CONFIG_RES_CONFIG_FILE_PATH
#define CONFIG_MAX_SIZE CONFIG_RES_CONFIG_FILE_MAX_SIZE
#define LOG_FILE        MOUNT_POINT CONFIG_RES_LOG_FILE_PATH
#define LOG_MAX_SIZE    CONFIG_RES_LOG_FILE_MAX_SIZE

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
 * @param epoch	 		the Unix epoch in seconds since 1 January 1970
 * @param timezone_str  the appropriate time zone per POSIX TZ (null for UTC)
 */
void set_system_time(const time_t epoch, const char* timezone_str);

/**
 * Gets the system time as the Unix epoch. Sets a timeval struct if it was
 * passed.
 *
 * @param tv	an empty timeval to be set to get precise time.
 *
 * @return 		the time in seconds since 1 January 1970
 */
time_t get_system_time(struct timeval *tv = nullptr);

/**
 * Starts a timer in one-shot mode to go off at the next 5 minute interval, plus
 * mod_ms milliseconds.
 *
 * @param timer		a reference to an active, non-running timer
 * @param mod_ms	some offset (positive or negative) in milliseconds
 *
 * @return 			the time until the timer expires in milliseconds
 */
int64_t set_window_wait_timer(esp_timer_handle_t &timer, const int mod_ms);

/**
 * Create a hexadecimal representation of the source string and store it in the
 * destination string. The hexadecimal string is separated into 8 bit segments
 * with a space. For example, the source string 'Hello world!' returns
 * '0x48 0x65 0x6C 0x6C 0x6F 0x20 0x77 0x6F 0x72 0x6C 0x64'.
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
 * returns '0x48 0x65 0x6C 0x6C 0x6F 0x20 0x77 0x6F 0x72 0x6C 0x64'.
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
 * Takes a time in milliseconds and turns it into a string representation like
 * 'MM:SS.mmm'
 *
 * @note you should allocate a string of 10 characters for the destination
 *
 * @param destination	the destination string
 * @param millis		the number of milliseconds to stringify
 *
 * @return				a pointer to the destination string
 */
char *ms2str(char* destination, int64_t millis);

/**
 * Returns the file size in bytes of a file using its file descriptor.
 *
 * @param fd		the file descriptor of a file
 *
 * @return 			the size of the file in bytes
 */
long fsize(FILE *fd);

esp_err_t get_config_resource(cJSON *&root);

esp_err_t set_config_resource(cJSON *root);

#endif /* MAIN_HELPERS_H_ */
