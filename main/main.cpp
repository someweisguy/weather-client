#include "main.h"

static const char *TAG { "main" };
static char *ssid, *wifi_pass, *mqtt_topic, *mqtt_broker, *timezone;
static Sensor* sensors[] { new BME280() };
static SemaphoreHandle_t backlog_semaphore;

static void set_log_levels() {
	esp_log_level_set("*", ESP_LOG_WARN);
	esp_log_level_set("sdcard", ESP_LOG_DEBUG);
	esp_log_level_set("wlan", ESP_LOG_DEBUG);
	esp_log_level_set("mqtt", ESP_LOG_DEBUG);

	esp_log_level_set("i2c", ESP_LOG_WARN);
	esp_log_level_set("uart", ESP_LOG_WARN);

	esp_log_level_set("ds3231", ESP_LOG_INFO);

	esp_log_level_set("main", ESP_LOG_VERBOSE);
	esp_log_level_set("helpers", ESP_LOG_VERBOSE);
	//esp_log_level_set("http", ESP_LOG_VERBOSE);
}

extern "C" void app_main() {
	esp_log_set_vprintf(vlogf);
	set_log_levels();

	setup_required_services();

	// TODO: Write code to read the log file over http

	// TODO: clean up http source
	// TODO: figure out how to download log file over http

	// Mount the SD card - deny service until mounted
	while (!sdcard_mount()) {
		ESP_LOGW(TAG, "Insert SD card to continue");
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

	// Load the config file values into memory
	ESP_LOGD(TAG, "Loading config file values into memory");
	FILE *fd { fopen(CONFIG_FILE_PATH, "r") };
	if (fd != nullptr) {
		// Read the JSON file into memory
		ESP_LOGV(TAG, "Reading config file into memory");
		const long size { fsize(fd) };
		if (size > 1024)
			ESP_LOGW(TAG, "Config file is larger than expected (%li bytes)", size);
		char file_str[size + 1];
		fread(file_str, 1, size, fd);
		fclose(fd);

		// Parse the JSON
		ESP_LOGV(TAG, "Parsing JSON file");
		cJSON *config { cJSON_Parse(file_str) };
		ssid = strdup(cJSON_GetObjectItem(config, "ssid")->valuestring);
		wifi_pass = strdup(cJSON_GetObjectItem(config, "pass")->valuestring);
		mqtt_topic = strdup(cJSON_GetObjectItem(config, "topic")->valuestring);
		mqtt_broker = strdup(cJSON_GetObjectItem(config, "mqtt")->valuestring);
		timezone = strdup(cJSON_GetObjectItem(config, "tz")->valuestring);
		cJSON_Delete(config);
	} else {
		ESP_LOGE(TAG, "Unable to load config file");
		while (true);
	}

	// Set the system time if possible
	bool time_is_synchronized { false };
	if (!ds3231_lost_power()) {
		set_system_time(ds3231_get_time(), timezone);
		time_is_synchronized = true;
	}

	// Connect to WiFi and MQTT
	wlan_connect(ssid, wifi_pass);
	mqtt_connect(mqtt_broker);

	// Synchronize system time with time server
	while (!time_is_synchronized) {
		wlan_block_until_connected(5000); // block 5 seconds
		if (wlan_connected() && sntp_synchronize_system_time(timezone))
			time_is_synchronized = ds3231_set_time();
	}

	// Create the synchronize system time task
	ESP_LOGD(TAG, "Starting system time synchronization task");
	xTaskCreate(synchronize_system_time_task, "synchronize_system_time", 2048,
			&time_is_synchronized, tskIDLE_PRIORITY, nullptr);

	// Create the send backlog task
	ESP_LOGD(TAG, "Starting data backlog monitor task");
	backlog_semaphore = xSemaphoreCreateBinary();
	xTaskCreate(send_backlog_task, "send_backlog", 4096, nullptr,
			tskIDLE_PRIORITY, nullptr);

	// Check if there is data to be written
	ESP_LOGD(TAG, "Checking for backlogged data");
	if (access("/sdcard/data.txt", F_OK) != -1)
		xSemaphoreGive(backlog_semaphore);

	// Do initial sensor setup
	ESP_LOGI(TAG, "Setting up the sensors");
	for (Sensor *sensor : sensors)
		sensor->setup();

	// Calculate the next sensor ready time
	TickType_t last_tick { xTaskGetTickCount() };
	int offset_ms { -SENSOR_READY_MS - 500 };
	time_t wait_ms { get_window_wait_ms(offset_ms) };
	if (wait_ms > 5000) // sleep if there more than 5 seconds to wait
		xTaskCreate(sensor_sleep_task, "sensor_sleep_task", 2048, nullptr,
				tskIDLE_PRIORITY, nullptr);
	vTaskDelayUntil(&last_tick, wait_ms / portTICK_PERIOD_MS);

	while (true) {
		ESP_LOGI(TAG, "Readying sensors");
		for (Sensor *sensor : sensors)
			sensor->wakeup();
		
		// Prepare the JSON root, time, and data object
		ESP_LOGV(TAG, "Constructing the JSON object");
		const time_t epoch = get_system_time();
		cJSON *json_root { cJSON_CreateObject() }, *json_data;
		cJSON_AddNumberToObject(json_root, "time", epoch + (300 - epoch % 300));
		cJSON_AddItemToObject(json_root, "data", json_data=cJSON_CreateObject());

		// Calculate next wake time
		ESP_LOGV(TAG, "Calculating next wake time");
		last_tick = xTaskGetTickCount();
		offset_ms = 0;
		wait_ms = get_window_wait_ms(offset_ms);
		vTaskDelayUntil(&last_tick, wait_ms / portTICK_PERIOD_MS);

		// Get the weather data
		ESP_LOGI(TAG, "Getting weather data");
		for (Sensor *sensor : sensors)
			sensor->get_data(json_data);

		// Create sensor sleep task to sleep sensor after 1 second
		xTaskCreate(sensor_sleep_task, "sensor_sleep", 2048, nullptr,
				tskIDLE_PRIORITY, nullptr);

		// Delete the JSON root
		ESP_LOGV(TAG, "Converting the JSON object to a string");
		char *json_str { cJSON_PrintUnformatted(json_root) };
		ESP_LOGV(TAG, "Deleting the JSON object");
		cJSON_Delete(json_root);

		// Handle the JSON string
		if (!mqtt_publish(mqtt_topic, json_str)) {
			// Write the data to file
			ESP_LOGI(TAG, "Writing data to file");
			FILE *fd { fopen(DATA_FILE_PATH, "a+") };
			if (fd != nullptr) {
				if (fputs(json_str, fd) < 0 || fputc('\n', fd) != '\n')
					ESP_LOGE(TAG, "Unable to write data to file (cannot write to file)");
				else xSemaphoreGive(backlog_semaphore);
				fclose(fd);
			} else {
				ESP_LOGE(TAG, "Unable to write data to file (cannot open file)");
			}
		}
		delete[] json_str;

		// Calculate next wake time
		ESP_LOGV(TAG, "Calculating next wake time");
		last_tick = xTaskGetTickCount();
		offset_ms = -SENSOR_READY_MS - 500;
		wait_ms = get_window_wait_ms(offset_ms);
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

	// TODO: Register shutdown handler
}

int vlogf(const char *format, va_list arg) {
	// Get an appropriate sized buffer for the message
	const int len { vsnprintf(nullptr, 0, format, arg) };
	char message[len + 1];
	vsprintf(message, format, arg);

	// Open the file and delete it if it gets too big
	FILE *fd { fopen(LOG_FILE_PATH, "a+") };
	if (fd != nullptr && fsize(fd) > 100 * 1024) { // 100 KB
		fclose(fd);
		remove(LOG_FILE_PATH);
		fd = fopen(LOG_FILE_PATH, "a+");
	} else if (fd != nullptr) {
		// Print everything to file except ASCII color codes
		bool in_esc { false };
		for (int i = 0; i < len; ++i) {
			if (in_esc) {
				if (message[i] == 'm') in_esc = false;
				else continue;
			} else {
				if (message[i] == '\033') in_esc = true;
				else fputc(message[i], fd);
			}
		}
		fclose(fd);
	}

	// Print to stdout
	return fputs(message, stdout);
}

time_t get_window_wait_ms(const int modifier_ms) {
	// Calculate next wake time
	timeval tv;
	get_system_time(&tv);
	time_t window_delta_ms = (300 - tv.tv_sec % 300) * 1000 - (tv.tv_usec
			/ 1000) + modifier_ms;
	if (window_delta_ms < 0) {
		ESP_LOGD(TAG, "Skipping next measurement window");
		window_delta_ms += 5 * 60 * 1000; // 5 minutes
	}

	// Log results
	time_t millis { window_delta_ms };
	const int mins { millis / (60 * 1000) };
	millis %= 60 * 1000;
	const int secs { millis / 1000 };
	millis %= 1000;
	ESP_LOGD(TAG, "Next window is in %02i:%02i.%03li (%+i ms)", mins,
			secs, millis, modifier_ms);
	return window_delta_ms;
}

void synchronize_system_time_task(void *args) {
	bool time_is_synchronized { *static_cast<bool*>(args) };
	if (time_is_synchronized)
		ESP_LOGD(TAG, "Skipping initial SNTP server synchronization");
	while (true) {
		while (!time_is_synchronized) {
			if (wlan_connected())
				time_is_synchronized = sntp_synchronize_system_time(timezone);
			else {
				ESP_LOGW(TAG, "Unable to synchronize system time (not connected)");
				vTaskDelay((60 * 1000) / portTICK_PERIOD_MS); // 1 minute
			}
		};
		vTaskDelay((7 * 24 * 60 * 60 * 1000) / portTICK_PERIOD_MS); // 1 week
		time_is_synchronized = false;
	}
}

void sensor_sleep_task(void *args) {
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "Putting sensors to sleep");
	for (Sensor *sensor : sensors)
		sensor->sleep();
	vTaskDelete(nullptr);
}

void send_backlog_task(void *args) {
	int file_pos { 0 };
	TickType_t wait_ticks { portMAX_DELAY };
	while (true) {
		// Wait for semaphore and for MQTT to connect
		xSemaphoreTake(backlog_semaphore, wait_ticks);
		if (!mqtt_connected()) {
			wait_ticks = (10 * 1000) / portTICK_PERIOD_MS; // 10 seconds
			continue;
		}

		// Open the file and read from it
		ESP_LOGI(TAG, "Sending the backlogged data...");
		FILE *fd { fopen(DATA_FILE_PATH, "r") };
		if (fd != nullptr) {
			fseek(fd, file_pos, SEEK_CUR); // goto last fail
			while (!feof(fd)) {
				// Allocate an appropriately sized string and read into it
				ESP_LOGV(TAG, "Reading JSON string from file into memory");
				int c, line_len = 0;
				while ((c = fgetc(fd)) != '\n' && c != EOF)
					++line_len;
				if (c == EOF) break;
				fseek(fd, -line_len, SEEK_CUR);
				char json_str[line_len + 1];
				fgets(json_str, line_len, fd);
				fgetc(fd); // skip newline

				// Publish the string to MQTT broker
				ESP_LOGV(TAG, "Sending backlogged data to MQTT");
				if (!mqtt_publish(mqtt_topic, json_str)) {
					file_pos = ftell(fd) - (line_len + 1); // include newline
					ESP_LOGW(TAG, "Did not complete sending backlogged data (failed at %i)",
							file_pos);
					wait_ticks = 5000 / portTICK_PERIOD_MS;
					break;
				}
			}

			// Remove the file if we reached the end of data
			if (feof(fd)) {
				ESP_LOGI(TAG, "Backlogged data was sent");
				fclose(fd);
				remove(DATA_FILE_PATH);
				file_pos = 0;
				wait_ticks = portMAX_DELAY;
			} else fclose(fd);
		} else {
			ESP_LOGE(TAG, "Unable to read data from file (cannot open file)");
		}
	}
}
