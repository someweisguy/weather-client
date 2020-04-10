/*
 * bsp.cpp
 *
 *  Created on: Apr 4, 2020
 *      Author: Mitch
 */

#include "bsp.h"
static const char* TAG { "bsp" };
static EventGroupHandle_t sntp_event_group;
static const EventBits_t SNTP_SUCCESS { 0x1 };

void sntp_event_handler(timeval *tv) {
	const double unix_time { tv->tv_sec + (tv->tv_usec / 1000000.0) };
	verbose(TAG, "Received unix time from NTP server: %.5f", unix_time);
	xEventGroupSetBits(sntp_event_group, SNTP_SUCCESS);
}

const char* esp_reset_to_name(esp_reset_reason_t code) {
	switch (code) {
	case ESP_RST_POWERON: return "power-on event";
	case ESP_RST_EXT: return "external pin reset";
	case ESP_RST_SW: return "software API reset";
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

esp_err_t sync_ntp_time(const char* ntp_server) {

	sntp_event_group = xEventGroupCreate();

	// Configure and initialize the SNTP driver
	sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, ntp_server);
	sntp_set_time_sync_notification_cb(&sntp_event_handler);
	verbose(TAG, "Connecting to \"%s\"", ntp_server);
	sntp_init();

	// Wait up to 10 seconds to connect to NTP server
	const EventBits_t sntp_ret { xEventGroupWaitBits(sntp_event_group,
				SNTP_SUCCESS, pdFALSE, pdFALSE, 10000 / portTICK_PERIOD_MS) };
	vEventGroupDelete(sntp_event_group);
	sntp_stop();

	if (sntp_ret == SNTP_SUCCESS) {
		return ESP_OK;
	} else {
		return ESP_FAIL;
	}
}

void set_cpu_time(const time_t unix_time) {
	timeval tv;
	tv.tv_sec = unix_time;
	tv.tv_usec = 0;
	settimeofday(&tv, nullptr);
}

time_t get_cpu_time() {
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

