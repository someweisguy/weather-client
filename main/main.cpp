
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstdio>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"

#include "i2c.h"
#include "uart.h"
#include "ds3231.h"
#include "sdcard.h"

#include "wlan.h"
#include "http.h"
#include "mqtt.h"

#include "helpers.h"

#include "Sensor.h"
#include "BME280.h"
//#include "PMS5003.h"

#define SENSOR_READY_SEC    30 /* Longest time (in seconds) that it takes for sensors to wake up */
#define BOOT_DELAY_SEC      5  /* Time (in seconds) that it takes the ESP32 to wake up */
#define BOOT_LOG_SIZE_BYTES 4096

#define LOG_FILE_NAME       "/sdcard/events.log"
#define CONFIG_FILE_NAME    "/sdcard/config.json"
#define DATA_FILE_NAME      "/sdcard/data.txt"

#define TIME_BETWEEN_RTC_SYNC_SEC 604800 // 7 days
#define NTP_SERVER "pool.ntp.org"


static const char *TAG { "main" };
//static RTC_DATA_ATTR wakeup_reason_t wakeup_reason { UNEXPECTED_REASON };
static Sensor* sensors[] { new BME280() };

extern "C" void app_main() {

	esp_log_level_set("*", ESP_LOG_NONE);
	esp_log_level_set("sdcard", ESP_LOG_DEBUG);
	esp_log_level_set("wlan", ESP_LOG_DEBUG);
	esp_log_level_set("mqtt", ESP_LOG_DEBUG);

	esp_log_level_set("i2c", ESP_LOG_WARN);
	esp_log_level_set("uart", ESP_LOG_WARN);

	esp_log_level_set("ds3231", ESP_LOG_INFO);

	esp_log_level_set("main", ESP_LOG_VERBOSE);
	//esp_log_level_set("power", ESP_LOG_VERBOSE);
	//esp_log_level_set("http", ESP_LOG_VERBOSE);

	esp_log_set_vprintf(vlogf);

	// Mount the SD card
	if (!sdcard_mount()) {
		ESP_LOGE(TAG, "Unable to mount the SD card");
		abort();
	}

	esp_err_t setup_ret;
	if ((setup_ret = nvs_flash_init()) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to initialize NVS flash (%i)", setup_ret);
		abort();
	}
	if ((setup_ret = esp_netif_init()) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to initialize network interface (%i)", setup_ret);
		abort();
	}
	if ((setup_ret = esp_event_loop_create_default()) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to create default event loop (%i)", setup_ret);
		abort();
	}

	if (!uart_start()) {
		ESP_LOGE(TAG, "Unable to start the UART port");
		abort();
	}

	if (!i2c_start()) {
		ESP_LOGE(TAG, "Unable to start the I2C port");
		abort();
	}

	// TODO: documentation about setting i2c to log level info or above for
	//  most accurate time sync in ds3231
	// TODO: Get rid of all %x in error logging functions
	//  Code that may occasionally throw fail should have strings to explain
	//  reasons for failure. Code that shouldn't fail may display errors as %i
	// TODO: clean up helper functions
	// TODO: Reimplement the old sd card function in the txt file on desktop
	//  TODO: read config file from sd card
	//  TODO: write string to file
	// TODO: clean up http source
	// TODO: figure out how to download log file over http
	// TODO: figure out how to store data to NVS and store it after successfully
	//  reading it from SD card
	// TODO: documentation for all functions

	// Do initial sensor setup
	bool time_is_synchronized { false };

	// Set the system time if possible
	if (!ds3231_lost_power()) {
		set_system_time(ds3231_get_time());
		time_is_synchronized = true;
	}

	// Connect to WiFi and MQTT
	wlan_connect("ESPTestNetwork", "ThisIsMyTestNetwork!");
	mqtt_connect("mqtt://192.168.0.2");

	// Synchronize system time with time server
	do {
		wlan_block_until_connected();
		if (wlan_connected() && sntp_synchronize_system_time())
			time_is_synchronized = ds3231_set_time();

		// Deny service if we are unable to synchronize the system time
		if (!time_is_synchronized) {
			// Re-check WiFi and MQTT credentials
			wlan_connect("ESPTestNetwork", "ThisIsMyTestNetwork!");
			mqtt_connect("mqtt://192.168.0.2");
		}
	} while (!time_is_synchronized);

	// Do initial sensor setup then sleep the sensors
	for (Sensor *sensor : sensors)
		sensor->setup();
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	for (Sensor *sensor : sensors)
		sensor->sleep();

	// Figure out the next wakeup time

	while (true) {
			ESP_LOGD(TAG, "Doing main loop");

			const char *greeting { "Hello world!" };
			uart_write(greeting, strlen(greeting));


			ds3231_get_time();

			vTaskDelay(15000 / portTICK_PERIOD_MS);



	}


}

