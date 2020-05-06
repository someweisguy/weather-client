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

#define LOG_LEVEL           INFO
#define SENSOR_READY_SEC    30 /* Longest time (in seconds) that it takes for sensors to wake up */
#define BOOT_DELAY_SEC      5  /* Time (in seconds) that it takes the ESP32 to wake up */
#define BOOT_LOG_SIZE_BYTES 4096

#define SD_MOUNT_POINT      "/sdcard"
#define LOG_FILE_NAME       "/events.log"
#define CONFIG_FILE_NAME    "/config.json"
#define DATA_FILE_NAME      "/data.txt"

#define TIME_BETWEEN_RTC_SYNC_SEC 604800 // 7 days
#define NTP_SERVER "pool.ntp.org"

static const char* TAG { "main" };
static RTC_DATA_ATTR wakeup_reason_t wakeup_reason { UNEXPECTED_REASON };
static RTC_DATA_ATTR time_t measurement_time { 0 }, next_rtc_sync { 0 };
static Sensor* sensors[] { }; //new BME280, new PMS5003 };

static const EventBits_t SENSOR_SLEEP { 0x1 };
static EventGroupHandle_t main_event_group;

bool saved_data_exists; // is there JSON data on the SD card
config_t config; // values stored in config file

esp_err_t send_boot_log() {
	ESP_ERROR_CHECK(mqtt_publish(config.mqtt_boot_log_topic, "START"));
	FILE *f { sdcard_open_file_readonly(LOG_FILE_NAME) };
	if (f != nullptr) {
		fseek(f, 0, SEEK_END);
		const int64_t file_size { ftell(f) };
		const int64_t boot_log_size { file_size < BOOT_LOG_SIZE_BYTES ?
				file_size : BOOT_LOG_SIZE_BYTES };
		fseek(f, file_size - boot_log_size, SEEK_SET);
		while (fgetc(f) != '\n'); // advance to the nearest full line
		while (getc(f) != EOF) {
			// Read a line from the file into memory
			const int32_t line_len { get_line_length(f) };
			char boot_log_str[line_len + 1];
			fgets(boot_log_str, line_len + 1, f);

			if (mqtt_publish(config.mqtt_boot_log_topic, boot_log_str) != ESP_OK) {
				fclose(f);
				error(TAG, "An error occurred while sending the boot log");
				return ESP_FAIL;
			}
		}
		fclose(f);
	}
	return ESP_OK;
}

bool connect_wifi() {
	if (!wifi_connected()) {
		info(TAG, "Connecting to WiFi...");
		if (wifi_connect_block(config.wifi_ssid, config.wifi_password) == ESP_OK) {
			info(TAG, "Connected to SSID \"%s\"", config.wifi_ssid);
		} else {
			warning(TAG, "Could not connect to SSID \"%s\"", config.wifi_ssid);
			// FIXME: free memory on failure
			//wifi_stop();
		}
	}
	return wifi_connected();
}

bool connect_mqtt() {
	if (wifi_connected() && !mqtt_connected()) {
		info(TAG, "Connecting to MQTT...");
		if (mqtt_connect(config.mqtt_broker) == ESP_OK) {
			info(TAG, "Connected to MQTT broker \"%s\"", config.mqtt_broker);
		} else {
			warning(TAG, "Could not connect to MQTT broker \"%s\"",
					config.mqtt_broker);
			// FIXME: free memory on failure
			//mqtt_stop();
		}
	}
	return mqtt_connected();
}

void sleep_sensor_task(void *pvParameters) {
	// Wait one second
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	// Put the sensors to sleep
	info(TAG, "Putting sensors to sleep");
	for (Sensor *sensor : sensors) {
		const esp_err_t sleep_ret { sensor->sleep() };
		if (sleep_ret != ESP_OK)
			error(TAG, "Could not put %s to sleep: %s", sensor->get_name(),
					esp_err_to_name(sleep_ret));
	}

	// Report that the task is complete
	xEventGroupSetBits(main_event_group, SENSOR_SLEEP);;
}

extern "C" void app_main() {

	esp_log_level_set("MQTT_CLIENT", ESP_LOG_NONE);
	esp_log_level_set("OUTBOX", ESP_LOG_NONE);
	esp_log_level_set("event", ESP_LOG_NONE);
	esp_log_level_set("wpa", ESP_LOG_NONE);
	esp_log_level_set("wifi", ESP_LOG_NONE);
	esp_log_level_set("efuse", ESP_LOG_NONE);
	esp_log_level_set("nvs", ESP_LOG_NONE);
	esp_log_level_set("tcpip_adapter", ESP_LOG_NONE);
	esp_log_level_set("*", ESP_LOG_NONE);

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
		if (wakeup_reason == ESP_RST_POWERON) {
			info(TAG, "Woke up due to power on event");
		} else {
			error(TAG, "Woke up for an unexpected reason (%s)",
					esp_reset_to_name(esp_reset_reason()));
		}

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

		// Send the boot log
		if (!wifi_connected() || !connect_mqtt() || send_boot_log() != ESP_OK)
			error(TAG, "Could not send boot log");

		// Perform initial setup of sensors
		info(TAG, "Resetting sensors to a known configuration");
		for (Sensor* sensor : sensors) {
			const esp_err_t setup_ret { sensor->setup() };
			if (setup_ret != ESP_OK)
				error(TAG, "Unable to setup %s: %s", sensor->get_name(),
						esp_err_to_name(setup_ret));
		}

		// Wait 1 second for setup to complete, then put sensors to sleep
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		info(TAG, "Putting sensors to sleep");
		for (Sensor *sensor : sensors) {
			const esp_err_t sleep_ret { sensor->sleep() };
			if (sleep_ret != ESP_OK)
				error(TAG, "Could not put %s to sleep: %s", sensor->get_name(),
						esp_err_to_name(sleep_ret));
		}


	} else if (wakeup_reason == READY_SENSORS) {

		// Log wakeup
		info(TAG, "Woke up to get the sensing hardware ready");

		// Wake up sensors
		info(TAG, "Waking up sensors");
		for (Sensor *sensor : sensors) {
			const esp_err_t wakeup_ret { sensor->wakeup() };
			if (wakeup_ret != ESP_OK)
				error(TAG, "Unable to wake up %s: %s", sensor->get_name(),
						esp_err_to_name(wakeup_ret));
		}
		if (measurement_time - SENSOR_READY_SEC < get_cpu_time())
			warning(TAG, "Sensors were not woken up before the deadline");

	} else { // wakeup_reason == TAKE_MEASUREMENT

		// Log wakeup
		info(TAG, "Woke up to take weather data measurements");

		// Create the main event group
		main_event_group = xEventGroupCreate();

		// Connect to WiFi without blocking
		info(TAG, "Connecting to WiFi without blocking");
		wifi_connect(config.wifi_ssid, config.wifi_password);

		// Ready sensors to take measurements
		debug(TAG, "Readying sensors to take data");
		for (Sensor *sensor : sensors) {
			const esp_err_t ready_ret { sensor->ready() };
			if (ready_ret != ESP_OK)
				error(TAG, "Unable to ready %s for data: %s", sensor->get_name(),
							esp_err_to_name(ready_ret));
		}

		// Build a JSON root, time, and data object
		cJSON *json_root { cJSON_CreateObject() }, *json_data;

		// Create JSON time object
		cJSON_AddNumberToObject(json_root, "time", measurement_time);

		// Prepare JSON data object
		cJSON_AddItemToObject(json_root, "data", json_data=cJSON_CreateObject());

		// Track which sensors didn't get data
		const size_t num_sensors { sizeof(sensors) / sizeof(Sensor) };
		esp_err_t sensor_rets[num_sensors];

		// Wait for the measurement window
		info(TAG, "Waiting for measurement window...");
		while (get_cpu_time() < measurement_time)
			vTaskDelay(10 / portTICK_PERIOD_MS);

		// Get weather data, log errors after all data has been taken
		info(TAG, "Taking weather measurements");
		for (int i = 0; i < num_sensors; ++i)
				sensor_rets[i] = sensors[i]->get_data(json_data);

		// Check to make sure data was measured on time
		const time_t actual_measurement_time { get_cpu_time() };
		if (measurement_time < actual_measurement_time)
			warning(TAG, "Data was not measured before the deadline");

		// Start a task to sleep the sensors
		TaskHandle_t sensor_sleep_task;
		xTaskCreate(sleep_sensor_task, "sensor_sleep", 2048, nullptr,
				tskIDLE_PRIORITY, &sensor_sleep_task);

		// Check if any sensors were not able to get data
		for (int i = 0; i < num_sensors; ++i) {
			if (sensor_rets[i] != ESP_OK)
				error(TAG, "Could not get data from %s: %s",
						sensors[i]->get_name(),
						esp_err_to_name(sensor_rets[i]));
		}

		// Build the JSON string for MQTT and delete the JSON object
		char *json_str { cJSON_Print(json_root) };
		strip(json_str); // remove newlines
		debug(TAG, "Got JSON: %s", json_str);
		cJSON_Delete(json_root);

		// Wait until we connect or fail to connect to WiFi
		wifi_block_until_connected();

		bool data_published { false };
		if (wifi_connected() && connect_mqtt()) {
			info(TAG, "Publishing to MQTT broker");
			if (mqtt_publish(config.mqtt_data_topic, json_str) == ESP_OK)
				data_published = true;
			else
				error(TAG, "Could not publish the JSON string to MQTT");
		}
		if (!data_published) {
			// Save JSON to file
			info(TAG, "Storing sensor data to file");
			ESP_ERROR_CHECK(store_json_string(DATA_FILE_NAME, json_str));
		}

		// Wait until the sensors have been put to sleep
		xEventGroupWaitBits(main_event_group, SENSOR_SLEEP, pdFALSE, pdFALSE,
						portMAX_DELAY);
		vTaskDelete(sensor_sleep_task);

		// Check if it is time to resync the DS3231
		if (get_cpu_time() > next_rtc_sync && wifi_connected()) {
			info(TAG, "Synchronizing onboard real-time clock...");
			debug(TAG, "Connecting to NTP server");
			if (sync_ntp_time(NTP_SERVER) == ESP_OK) {
				ESP_ERROR_CHECK(ds3231_set_time(get_cpu_time()));
				next_rtc_sync = get_cpu_time() + TIME_BETWEEN_RTC_SYNC_SEC;
			} else {
				warning(TAG, "Could not connect to the NTP server");
			}
		} else if (!wifi_connected()) {
			warning(TAG, "Overdue for onboard real-time clock synchronization");
		}

	}



	// Check if there is backlogged data to transmit to MQTT
	if (saved_data_exists && wakeup_reason != READY_SENSORS) {
		info(TAG, "Found saved sensor data to send to MQTT");

		// Connect to MQTT if we haven't already
		if (wakeup_reason == UNEXPECTED_REASON)
			connect_mqtt();

		// Publish saved sensor data to MQTT line by line
		if (wifi_connected() && mqtt_connected()) {
			info(TAG, "Publishing backlogged sensor data to MQTT");
			FILE *f { sdcard_open_file_readonly(DATA_FILE_NAME) };
			do {
				// Read a line from the file into memory
				const int32_t line_len { get_line_length(f) };
				char json_str[line_len + 1];
				fgets(json_str, line_len + 1, f);
				if (mqtt_publish(config.mqtt_data_topic, json_str) != ESP_OK) {
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
	if (wifi_connected()) {
		debug(TAG, "Stopping wireless services");
		// Stop MQTT
		if (mqtt_connected()) {
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
			next_wake_tm->tm_mon + 1, next_wake_tm->tm_mday,
			next_wake_tm->tm_year + 1900, next_wake_tm->tm_hour,
			next_wake_tm->tm_min, next_wake_tm->tm_sec);
	const time_t deep_sleep_sec { next_wake_time - get_cpu_time() };
	const time_t m { deep_sleep_sec / 60 }, s { deep_sleep_sec % 60 };
	debug(TAG, "Unmounting SD card and going to deep sleep for %u minute(s) "
			"and %u second(s)", m, s);
	ESP_ERROR_CHECK(sdcard_unmount());
	esp_sleep_enable_timer_wakeup(deep_sleep_sec * 1000000); // uS
	esp_deep_sleep_start();
}
