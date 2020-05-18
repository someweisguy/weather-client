#include "main.h"

static const char *TAG { "main" };
static Sensor* sensors[] { new BME280() };

static void set_log_levels() {
	esp_log_level_set("*", ESP_LOG_NONE);
	esp_log_level_set("sdcard", ESP_LOG_DEBUG);
	esp_log_level_set("wlan", ESP_LOG_DEBUG);
	esp_log_level_set("mqtt", ESP_LOG_DEBUG);

	esp_log_level_set("i2c", ESP_LOG_WARN);
	esp_log_level_set("uart", ESP_LOG_WARN);

	esp_log_level_set("ds3231", ESP_LOG_INFO);

	esp_log_level_set("main", ESP_LOG_VERBOSE);
	esp_log_level_set("helpers", ESP_LOG_VERBOSE);
	//esp_log_level_set("power", ESP_LOG_VERBOSE);
	//esp_log_level_set("http", ESP_LOG_VERBOSE);
}

extern "C" void app_main() {
	esp_log_set_vprintf(vlogf);
	set_log_levels();

	// Mount the SD card - deny service until mounted
	while (!sdcard_mount()) {
		ESP_LOGE(TAG, "Unable to mount the SD card");
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

	setup_required_services();

	// TODO: clean up helper functions
	// TODO: Reimplement the old sd card function in the txt file on desktop
	//  TODO: read config file from sd card
	//  TODO: write string to file
	// TODO: clean up http source
	// TODO: figure out how to download log file over http
	// TODO: documentation for all functions
	// TODO: figure out how to manage event handlers for publishing MQTT data
	//  from file - there could be an issue if sending data and writing new data
	//  at the same time

	// Set the system time if possible
	bool time_is_synchronized { false };
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
		}
	} while (!time_is_synchronized);

	// Do initial sensor setup then sleep the sensors
	ESP_LOGI(TAG, "Setting up the sensors");
	for (Sensor *sensor : sensors)
		sensor->setup();

	// TODO: Start a task that periodically resyncs the clock

	// TODO: Attach event handler on MQTT connect that sends all of the
	//  backlogged data

	// Calculate the next sensor ready time
	TickType_t last_tick { xTaskGetTickCount() };
	int offset_ms { -SENSOR_READY_MS - 1000 };
	time_t wait_ms { get_wait_ms(offset_ms) };
	if (wait_ms > 1100) // sleep sensors if there is time
		xTaskCreate(sensor_sleep_task, "sensor_sleep", 2048, nullptr,
				tskIDLE_PRIORITY, nullptr);
	vTaskDelayUntil(&last_tick, wait_ms / portTICK_PERIOD_MS);

	while (true) {
		ESP_LOGI(TAG, "Readying sensors");
		for (Sensor *sensor : sensors)
			sensor->wakeup();
		
		// Prepare the JSON root, time, and data object
		ESP_LOGD(TAG, "Constructing the JSON object");
		const time_t epoch = get_system_time();
		cJSON *json_root { cJSON_CreateObject() }, *json_data;
		cJSON_AddNumberToObject(json_root, "time", epoch + (300 - epoch % 300));
		cJSON_AddItemToObject(json_root, "data", json_data=cJSON_CreateObject());

		// Calculate next wake time
		ESP_LOGV(TAG, "Calculating next wake time");
		last_tick = xTaskGetTickCount();
		offset_ms = 0;
		wait_ms = get_wait_ms(offset_ms);
		vTaskDelayUntil(&last_tick, wait_ms / portTICK_PERIOD_MS);

		
		ESP_LOGI(TAG, "Getting weather data");
		for (Sensor *sensor : sensors)
			sensor->get_data(json_data);

		// Create sensor sleep task to sleep sensor after 1 second
		xTaskCreate(sensor_sleep_task, "sensor_sleep", 2048, nullptr,
				tskIDLE_PRIORITY, nullptr);

		// Delete the JSON root
		ESP_LOGD(TAG, "Converting the JSON object to a string");
		ESP_LOGV(TAG, "Printing JSON to string");
		char *json_str { cJSON_Print(json_root) };
		ESP_LOGV(TAG, "Deleting the JSON object");
		cJSON_Delete(json_root);

		// Handle the JSON string
		bool json_published { false };
		if (mqtt_connected())
			json_published = mqtt_publish("/test/weather/data", json_str);
		if (!json_published) {
			// Write the data to file
			ESP_LOGI(TAG, "Writing data to file");
			FILE *fd { fopen(DATA_FILE_PATH, "a+") };
			if (fd != nullptr) {
				// Strip the JSON string and print it to file
				if (fputs(strip(json_str), fd) < 0 || fputc('\n', fd) != '\n')
					ESP_LOGE(TAG, "Unable to write data to file (cannot write to file)");
				fclose(fd);
			} else {
				ESP_LOGE(TAG, "Unable to write data to file (cannot open file)");
			}
		}
		delete[] json_str;

		// Calculate next wake time
		ESP_LOGV(TAG, "Calculating next wake time");
		last_tick = xTaskGetTickCount();
		offset_ms = -SENSOR_READY_MS - 1000;
		wait_ms = get_wait_ms(offset_ms);
		vTaskDelayUntil(&last_tick, wait_ms / portTICK_PERIOD_MS);
	}
}

void setup_required_services() {
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
}

void sensor_sleep_task(void *args) {
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "Putting sensors to sleep");
	for (Sensor *sensor : sensors)
		sensor->sleep();
	vTaskDelete(nullptr);
}
