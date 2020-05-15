
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
#include <sdcard/sdcard.h>

#include "wlan.h"
#include "http.h"
#include "mqtt.h"

#include "helpers.h"

#include "Sensor.h"
#include "BME280.h"
#include "PMS5003.h"


#define SENSOR_READY_SEC    30 /* Longest time (in seconds) that it takes for sensors to wake up */
#define BOOT_DELAY_SEC      5  /* Time (in seconds) that it takes the ESP32 to wake up */
#define BOOT_LOG_SIZE_BYTES 4096

#define LOG_FILE_NAME       "/sdcard/events.log"
#define CONFIG_FILE_NAME    "/sdcard/config.json"
#define DATA_FILE_NAME      "/sdcard/data.txt"

#define TIME_BETWEEN_RTC_SYNC_SEC 604800 // 7 days
#define NTP_SERVER "pool.ntp.org"


static const char *TAG { "main" };
static RTC_DATA_ATTR wakeup_reason_t wakeup_reason { UNEXPECTED_REASON };

static volatile bool on_battery_power { false };

static void IRAM_ATTR gpio_isr_handler(void *args) {
	on_battery_power = !gpio_get_level(GPIO_NUM_34);
	gpio_set_level(GPIO_NUM_13, on_battery_power);
}

void configure_low_power_interrupt() {
	const gpio_num_t LED = GPIO_NUM_13, INTR = GPIO_NUM_34;

	// Setup LED
	gpio_pad_select_gpio(LED);
	gpio_set_direction(LED, GPIO_MODE_OUTPUT);

	// Setup interrupt pin
	gpio_set_direction(INTR, GPIO_MODE_INPUT);
	gpio_set_intr_type(INTR, GPIO_INTR_ANYEDGE);

	// Setup and enable the lowest priority interrupt
	gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
	gpio_isr_handler_add(INTR, gpio_isr_handler, nullptr);
	gpio_intr_enable(INTR);

	// Set the initial state
	gpio_isr_handler(nullptr);
}




extern "C" void app_main() {

	esp_log_level_set("*", ESP_LOG_NONE);
	esp_log_level_set("main", ESP_LOG_VERBOSE);
	esp_log_level_set("sdcard", ESP_LOG_VERBOSE);
	esp_log_level_set("wlan", ESP_LOG_VERBOSE);
	esp_log_level_set("mqtt", ESP_LOG_VERBOSE);
	esp_log_level_set("uart", ESP_LOG_DEBUG);
	esp_log_level_set("ds3231", ESP_LOG_VERBOSE);
	esp_log_level_set("i2c", ESP_LOG_INFO);
	//esp_log_level_set("http", ESP_LOG_VERBOSE);

	esp_log_set_vprintf(vlogf);

	configure_low_power_interrupt();

	// Mount the SD card
	if (sdcard_mount() != ESP_OK)
		ESP_LOGE(TAG, "Could not mount the SD card");

	// TODO: error check these functions
	nvs_flash_init();
	esp_netif_init();
	esp_event_loop_create_default();



	// TODO: documentation about setting i2c to log level info or above for
	//  most accurate time sync in ds3231
	// TODO: fix documentation for strhex and strnhex to len * 5
	// TODO: fix all uses of strhex or strnhex to len * 5
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
	// TODO: remove esp_err_t from returns in functions if possible

	bool system_time_is_synced = false;

	uart_start();
	i2c_start();


	while (1) {

		//const char *greeting { "Hello world!" };
		//uart_write(greeting, strlen(greeting));

		wlan_connect_and_block("ESPTestNetwork", "ThisIsMyTestNetwork!");
		mqtt_connect_and_block("mqtt://192.168.0.2");

		//vTaskDelay(8000 / portTICK_PERIOD_MS);

		if (wlan_connected() && !system_time_is_synced) {
			if (sntp_synchronize_system_time()) {
				system_time_is_synced = ds3231_set_time();
			}
		}
		//mqtt_publish("/test/esp", "Hello world!");

		time_t epoch { ds3231_get_time() };

		vTaskDelay(5000 / portTICK_PERIOD_MS);


	}

}
