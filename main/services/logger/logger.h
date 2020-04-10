/*
 * wifi.h
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#ifndef MAIN_SERVICES_LOGGER_LOGGER_H_
#define MAIN_SERVICES_LOGGER_LOGGER_H_

#include <iostream>
#include <cstdarg>
#include <cstring>
#include <time.h>
#include <sys/time.h>

#include "esp_sntp.h"
#include "esp_log.h"

enum log_level_t {
	VERBOSE = 0,
	DEBUG,
	INFO,
	WARNING,
	ERROR
};

esp_err_t logger_start(const char* log_file_name);
void logger_set_level(log_level_t level);

void verbose(const char* log_tag, const char *msg, ...);
void debug(const char* log_tag, const char *msg, ...);
void info(const char* log_tag, const char *msg, ...);
void warning(const char* log_tag, const char *msg, ...);
void error(const char* log_tag, const char *msg, ...);

#endif /* MAIN_SERVICES_LOGGER_LOGGER */
