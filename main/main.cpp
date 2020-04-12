#include <cstdio>

#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_sleep.h"

#include "bsp.h"
#include "i2c.h"
#include "uart.h"
#include "ds3231.h"
#include "sdcard.h"

#include "wifi.h"
#include "mqtt.h"

#include "Sensor.h"
#include "BME280.h"
#include "PMS5003.h"

#include "logger.h"

#define LOG_LEVEL        DEBUG
#define SENSOR_READY_SEC 30 /* Longest time (in seconds) that it takes for sensors to wake up */
#define BOOT_DELAY_SEC    5 /* Time (in seconds) that it takes the ESP32 to wake up */

#define SD_MOUNT_POINT   "/sdcard"
#define LOG_FILE_NAME    "/events.log"
#define CONFIG_FILE_NAME "/config.json"
#define DATA_FILE_NAME   "/data.txt"

#define TIME_BETWEEN_RTC_SYNC_SEC 604800 // 7 days
#define NTP_SERVER "pool.ntp.org"

static const char* TAG { "main" };
static RTC_DATA_ATTR wakeup_reason_t wakeup_reason { UNEXPECTED_REASON };
static RTC_DATA_ATTR time_t measurement_time { 0 }, next_rtc_sync { 0 };
static Sensor* sensors[] { new BME280, new PMS5003 };

bool wifi_connected { false }, mqtt_connected { false }, saved_data_exists;
config_t config; // values stored in config file

bool connect_wifi() {
	if (!wifi_connected) {
		info(TAG, "Connecting to WiFi...");
		if (wifi_connect(config.wifi_ssid, config.wifi_password) == ESP_OK) {
			info(TAG, "Connected to SSID \"%s\"", config.wifi_ssid);
			wifi_connected = true;
		} else {
			error(TAG, "Could not connect to SSID \"%s\"", config.wifi_ssid);
		}
	}
	return wifi_connected;
}

bool connect_mqtt() {
	if (wifi_connected && !mqtt_connected) {
		info(TAG, "Connecting to MQTT...");
		if (mqtt_connect(config.mqtt_broker) == ESP_OK) {
			info(TAG, "Connected to MQTT broker \"%s\"", config.mqtt_broker);
			mqtt_connected = true;
		} else {
			error(TAG, "Could not connect to MQTT broker \"%s\"",
					config.mqtt_broker);
		}
	}
	return mqtt_connected;
}

extern "C" void app_main() {

	logger_set_level(LOG_LEVEL);

	// Start ESP required services
	debug(TAG, "Starting ESP required services");
	verbose(TAG, "Initializing non-volatile flash storage");
	ESP_ERROR_CHECK(nvs_flash_init());
	verbose(TAG, "Initializing TCP/IP adapter");
	tcpip_adapter_init();
	verbose(TAG, "Creating default ESP event loop");
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// Start GNDCTRL required services
	debug(TAG, "Starting hardware services");
	verbose(TAG, "Mounting SD card");
	ESP_ERROR_CHECK(sdcard_mount(SD_MOUNT_POINT));
	ESP_ERROR_CHECK(logger_start(SD_MOUNT_POINT LOG_FILE_NAME));
	debug(TAG, "Begin GNDCTRL log");
	verbose(TAG, "Checking configuration file is formatted correctly");
	// TODO: ensure config file setup is correct
	verbose(TAG, "Starting i2c");
	ESP_ERROR_CHECK(i2c_start());
	verbose(TAG, "Starting UART");
	ESP_ERROR_CHECK(uart_start());



	// Read the configuration file values from SD card
	if (wakeup_reason != READY_SENSORS) {
		debug(TAG, "Reading configuration values from file");
		ESP_ERROR_CHECK(sdcard_get_config_vals(CONFIG_FILE_NAME, config));
		ESP_ERROR_CHECK(sdcard_file_exists(DATA_FILE_NAME, saved_data_exists));
	}



	if (wakeup_reason == UNEXPECTED_REASON) {
		// Log wakeup reason
		error(TAG, "Woke up for an unexpected reason (%s)",
				esp_reset_to_name(esp_reset_reason()));

		// Attempt to synchronize time with DS3231
		bool time_is_synchronized, lost_power;
		ESP_ERROR_CHECK(ds3231_lost_power(lost_power));
		if (!lost_power) {
			debug(TAG, "Recovering time from onboard real-time clock");
			time_t unix_time;
			ESP_ERROR_CHECK(ds3231_get_time(unix_time));
			set_cpu_time(unix_time);
			time_is_synchronized = true;
		} else {
			warning(TAG, "Onboard real-time clock is not accurate");
			time_is_synchronized = false;
		}

		// Synchronize real-time clock to NTP server
		if (connect_wifi()) {
			info(TAG, "Synchronizing onboard real-time clock...");
			debug(TAG, "Connecting to NTP server");
			if (sync_ntp_time(NTP_SERVER) == ESP_OK) {
				ESP_ERROR_CHECK(ds3231_set_time(get_cpu_time()));
				next_rtc_sync = get_cpu_time() + TIME_BETWEEN_RTC_SYNC_SEC;
				time_is_synchronized = true;
			} else {
				warning(TAG, "Could not connect to the NTP server");
			}
		}

		// Deny service if we are unable to synchronize the time
		if (!time_is_synchronized) {
			error(TAG, "Could not synchronize time");
			esp_restart();
		}

		// Perform initial setup of sensors
		info(TAG, "Resetting sensors to a known configuration");
		for (Sensor* sensor : sensors) {
			if (!sensor->setup())
				error(TAG, "Unable to setup %s", sensor->get_name());
		}

		// Wait 1 second for setup to complete, then put sensors to sleep
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		info(TAG, "Putting sensors to sleep");
		for (Sensor *sensor : sensors) {
			if (!sensor->sleep())
				error(TAG, "Could not put %s to sleep", sensor->get_name());
		}

	} else if (wakeup_reason == READY_SENSORS) {

		// Log wakeup
		info(TAG, "Woke up to get the sensing hardware ready");

		// Wake up sensors
		info(TAG, "Waking up sensors");
		for (Sensor *sensor : sensors) {
			if (!sensor->wakeup())
				error(TAG, "Unable to wake up %s", sensor->get_name());
		}
		if (measurement_time - SENSOR_READY_SEC < get_cpu_time())
			warning(TAG, "Sensors were not woken up before the deadline");

	} else { // wakeup_reason == TAKE_MEASUREMENT

		// Log wakeup
		info(TAG, "Woke up to take weather data measurements");

		// Ready sensors to take measurements
		debug(TAG, "Readying sensors to take data");
		for (Sensor *sensor : sensors) {
			if (!sensor->ready())
				error(TAG, "Unable to ready %s for data", sensor->get_name());
		}

		// Build a JSON root object
		cJSON *json_root { cJSON_CreateObject() }, *json_time;
		cJSON_AddItemToObject(json_root, "time", json_time=cJSON_CreateObject());
		char iso8601_str[20];
		strftime(iso8601_str, 20, "%F %T", localtime(&measurement_time));
		cJSON_AddStringToObject(json_time, "iso8601", iso8601_str);
		cJSON_AddNumberToObject(json_time, "unix", measurement_time);
		const size_t num_sensors { sizeof(sensors) / sizeof(Sensor) };
		bool bad_sensors[num_sensors] {}; // set to false by default

		// Wait for the measurement window
		info(TAG, "Waiting for measurement window...");
		while (get_cpu_time() < measurement_time)
			vTaskDelay(10 / portTICK_PERIOD_MS);

		// Get weather data, log errors after all data has been taken
		info(TAG, "Taking weather measurements");
		for (int i = 0; i < num_sensors; ++i) {
			if (!sensors[i]->get_data(json_root))
				bad_sensors[i] = true;
		}
		for (int i = 0; i < num_sensors; ++i) {
			if (bad_sensors[i])
				error(TAG, "Could not get data from %s", sensors[i]->
						get_name());
		}

		// Check to make sure data was measured on time
		const time_t actual_measurement_time { get_cpu_time() };
		if (measurement_time < actual_measurement_time)
			warning(TAG, "Data was not measured before the deadline");

		// Build the JSON string for MQTT and delete the JSON object
		char *json_str { cJSON_Print(json_root) };
		cJSON_Delete(json_root);

		// Wait for at least one second before sending the sleep command
		while (get_cpu_time() < actual_measurement_time + 1)
			vTaskDelay(10 / portTICK_PERIOD_MS);

		// Put the sensors to sleep
		info(TAG, "Putting sensors to sleep");
		for (Sensor *sensor : sensors) {
			if (!sensor->sleep())
				error(TAG, "Could not put %s to sleep", sensor->get_name());
		}

		bool data_published { false };
		if (connect_wifi() && connect_mqtt()) {
			debug(TAG, "Publishing to MQTT broker");
			if (mqtt_publish(config.mqtt_topic, json_str) == ESP_OK)
				data_published = true;
			else
				error(TAG, "Could not publish the JSON string to MQTT");
		}
		if (!data_published) {
			// Save JSON to file
			info(TAG, "Storing sensor data to file");
			strip(json_str); // remove newlines
			ESP_ERROR_CHECK(store_json_string(DATA_FILE_NAME, json_str));
		}

		// Check if it is time to resync the DS3231
		if (get_cpu_time() > next_rtc_sync && wifi_connected) {
			info(TAG, "Synchronizing onboard real-time clock...");
			debug(TAG, "Connecting to NTP server");
			if (sync_ntp_time(NTP_SERVER) == ESP_OK) {
				ESP_ERROR_CHECK(ds3231_set_time(get_cpu_time()));
				next_rtc_sync = get_cpu_time() + TIME_BETWEEN_RTC_SYNC_SEC;
			} else {
				warning(TAG, "Could not connect to the NTP server");
			}
		} else if (!wifi_connected) {
			warning(TAG, "Overdue for onboard real-time clock synchronization");
		}

	}



	// Check if there is backlogged data to transmit to MQTT
	if (saved_data_exists && wakeup_reason != READY_SENSORS) {
		// Publish saved sensor data to MQTT line by line
		if (connect_wifi() && connect_mqtt()) {
			info(TAG, "Publishing backlogged sensor data to MQTT");
			FILE *f { sdcard_open_file_readonly(DATA_FILE_NAME) };
			do {
				// Read a line from the file into memory
				int32_t line_len { get_line_length(f) };
				char json_str[line_len + 1];
				fgets(json_str, line_len + 1, f);
				if (mqtt_publish(config.mqtt_topic, json_str) != ESP_OK) {
					error(TAG, "Could not publish JSON string to "
							"MQTT");
					// FIXME: ensure data gets saved and not sent twice?
				}
			} while (fgetc(f) == '\n');
			fclose(f);
			sdcard_delete_file(DATA_FILE_NAME);
		}
	}



	// Calculate when we next need to wake up
	time_t next_wake_time;
	const time_t unix_time { get_cpu_time() };
	if (unix_time >= measurement_time) {
		measurement_time = unix_time + 300 - (unix_time % 300);
		debug(TAG, "Calculated a new measurement deadline");
	}

	// Get the next wakeup reason
	if (wakeup_reason == READY_SENSORS) {
		wakeup_reason = TAKE_MEASUREMENT;
		next_wake_time = measurement_time - BOOT_DELAY_SEC;
	} else { // wakeup_reason == TAKE_MEASUREMENT || UNEXPECTED_REASON
		wakeup_reason = READY_SENSORS;
		if (measurement_time - unix_time < SENSOR_READY_SEC + BOOT_DELAY_SEC) {
			warning(TAG, "Skipping the next measurement window");
			measurement_time += 300;
		}
		next_wake_time = measurement_time - SENSOR_READY_SEC - BOOT_DELAY_SEC;
	}



	// Stop wireless services
	if (wifi_connected) {
		debug(TAG, "Stopping wireless services");
		// Stop MQTT
		if (mqtt_connected) {
			verbose(TAG, "Stopping MQTT");
			if (mqtt_stop() != ESP_OK)
				warning(TAG, "Could not gracefully stop MQTT");
		}
		// Stop WiFi
		verbose(TAG, "Stopping WiFi");
		if (wifi_stop() != ESP_OK)
			warning(TAG, "Could not gracefully stop WiFi");
	}

	// Stop serial services
	debug(TAG, "Stopping hardware services");
	verbose(TAG, "Stopping UART");
	if (uart_stop() != ESP_OK)
		warning(TAG, "Could not gracefully stop UART");
	verbose(TAG, "Stopping i2c");
	if (i2c_stop() != ESP_OK)
		warning(TAG, "Could not gracefully stop i2c");

	// Log next wake time
	const tm *next_wake_tm { localtime(&next_wake_time) };
	info(TAG, "Sleeping until %02i/%02i/%i %02i:%02i:%02i",
			next_wake_tm->tm_mon, next_wake_tm->tm_mday,
			next_wake_tm->tm_year + 1900, next_wake_tm->tm_hour,
			next_wake_tm->tm_min, next_wake_tm->tm_sec);
	const time_t deep_sleep_sec { next_wake_time - get_cpu_time() };
	const time_t m { deep_sleep_sec / 60 }, s { deep_sleep_sec % 60 };
	debug(TAG, "Unmounting SD card and going to deep sleep for %u minute(s) "
			"and %u second(s)", m, s);
	ESP_ERROR_CHECK(sdcard_unmount());
	esp_deep_sleep(deep_sleep_sec * 1000000); // uS
}
