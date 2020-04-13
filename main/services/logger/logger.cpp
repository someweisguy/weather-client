/*
 * wifi.cpp
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#include "logger.h"
static const char* TAG { "logger" };

// Comment this out to stop logging to a console (this will speed up runtime)
//#define LOG_TO_CONSOLE

static log_level_t LEVEL { DEBUG };
static char *LOG_FILE_NAME { 0 };


static void write_log(const log_level_t log_level, const char *tag,
		const char *msg, va_list args) {

	// Only log message of greater importance than the current log level
	if (log_level < LEVEL)
		return;

	// The format for the prefix to the log message
	const char *log_prefix_format { "[%c] (%s) %s: " };

	// Get the time string
	char time_str[20];
	const time_t now { time(nullptr) };
	strftime(time_str, 20, "%F %T", localtime(&now));

	// Set the log character (and log color if printing to console)
	char log_char;
#ifdef LOG_TO_CONSOLE
	const char *log_color;
#endif
	switch (log_level) {
	case VERBOSE:
		log_char = 'V';
#ifdef LOG_TO_CONSOLE
		log_color = "\x1b[35m"; // magenta
#endif
		break;

	case DEBUG:
		log_char = 'D';
#ifdef LOG_TO_CONSOLE
		log_color = "\x1b[36m"; // cyan
#endif
		break;

	case INFO:
#ifdef LOG_TO_CONSOLE
		log_color = "\x1b[0m"; // white
#endif
		log_char = 'I';
		break;

	case WARNING:
#ifdef LOG_TO_CONSOLE
		log_color = "\x1b[33m"; // yellow
#endif
		log_char = 'W';
		break;

	case ERROR:
#ifdef LOG_TO_CONSOLE
		log_color = "\x1b[31m"; // red
#endif
		log_char = 'E';
		break;

	default:
#ifdef LOG_TO_CONSOLE
		log_color = "\x1b[31m"; // red
#endif
		log_char = '?';
		break;
	}

#ifdef LOG_TO_CONSOLE
	// Print the log entry to stdout
	fputs(log_color, stdout);
	printf(log_prefix_format, log_char,  time_str, tag);
	vprintf(msg, args);
	puts("\x1b[0m"); // white
#endif

	// Check that logger_start() was called
	if (LOG_FILE_NAME == 0)
		return;

	// Log to the SD card
	FILE *f { fopen(LOG_FILE_NAME, "a+") };
	if (f != nullptr) {
		fprintf(f, log_prefix_format, log_char, time_str, tag);
		vfprintf(f, msg, args);
		fputc('\n', f);
		fclose(f);
	}
}

esp_err_t logger_start(const char *log_file_name) {

	// Set the log file name
	LOG_FILE_NAME = new char[strlen(log_file_name)];
	strcpy(LOG_FILE_NAME, log_file_name);

	// Get info about the log file if it exists
	struct stat st;
	if (stat(log_file_name, &st) != 0)
		return ESP_OK;

	// Remove log file if it is larger than 10MB
	if (st.st_size > 1024 * 1024 * 10) {
		if (remove(log_file_name) != 0) {
			return ESP_FAIL;
		}
		debug(TAG, "Cleaned log file");
	}

	// Set the static log file variable
	return ESP_OK;
}

void logger_set_level(log_level_t level) {
	LEVEL = level;
}

void verbose(const char *log_tag, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	write_log(VERBOSE, log_tag, msg, args);
	va_end(args);
}

void debug(const char *log_tag, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	write_log(DEBUG, log_tag, msg, args);
	va_end(args);
}

void info(const char *tag, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	write_log(INFO, tag, msg, args);
	va_end(args);
}

void warning(const char *tag, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	write_log(WARNING, tag, msg, args);
	va_end(args);
}

void error(const char *tag, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	write_log(ERROR, tag, msg, args);
	va_end(args);
}
