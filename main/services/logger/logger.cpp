/*
 * wifi.cpp
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#include "logger.h"
static const char* TAG { "logger" };

// Comment this out to stop logging to a console (this will speed up runtimes)
#define LOG_TO_CONSOLE

// Console colors
#ifdef LOG_TO_CONSOLE
static const char *RED { "\x1b[31m" }, *YELLOW { "\x1b[33m" },
		*MAGENTA { "\x1b[35m" }, *CYAN { "\x1b[36m" }, *WHITE { "\x1b[0m" };
#endif

static log_level_t LEVEL { DEBUG };
static char* LOG_FILE_NAME { 0 };


static void write_log(const log_level_t log_level, const char *tag,
		const char *msg, va_list args) {

	// Only log message of greater importance than the current log level
	if (log_level < LEVEL)
		return;

	// Declare log format after the log abbreviation
	const char *format = " (%s) %s: ";

	// Set the log char
	char log_char;
	switch (log_level) {
	case VERBOSE:
		log_char = 'V';
		break;

	case DEBUG:
		log_char = 'D';
		break;

	case INFO:
		log_char = 'I';
		break;

	case WARNING:
		log_char = 'W';
		break;

	case ERROR:
		log_char = 'E';
		break;

	default:
		log_char = '?';
		break;
	}

	// Get the time
	time_t now = time(nullptr);
	tm *ptm = localtime(&now);
	const char *time_fmt = "%F %T";
	char time_str[21];
	strftime(time_str, 20, time_fmt, ptm);


#ifdef LOG_TO_CONSOLE
	// Set the console color
	const char* log_color;
	switch (log_level) {
	case VERBOSE:
		log_color = MAGENTA;
		break;

	case DEBUG:
		log_color = CYAN;
		break;

	case INFO:
	default:
		log_color = WHITE;
		break;

	case WARNING:
		log_color = YELLOW;
		break;

	case ERROR:
		log_color = RED;
		break;
	}

	// Print the log entry to stdout
	fputs(log_color, stdout);
	putchar('[');
	putchar(log_char);
	putchar(']');
	printf(format, time_str, tag);
	vprintf(msg, args);
	puts(WHITE);
#endif

	// Check that logger_start() was called
	if (LOG_FILE_NAME == 0)
		return;

	// Log to the SD card
	FILE *f = fopen(LOG_FILE_NAME, "a+");
	if (f != nullptr) {
		fputc(log_char, f);
		fprintf(f, format, time_str, tag);
		vfprintf(f, msg, args);
		fputc('\n', f);
		fclose(f);
	}
}

esp_err_t logger_start(const char* log_file_name) {
	// Get info about the log file
	struct stat st;
	if (stat(log_file_name, &st) != 0)
		return ESP_FAIL;

	// Set the log file name
 	LOG_FILE_NAME = new char[strlen(log_file_name)];
 	strcpy(LOG_FILE_NAME, log_file_name);

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
