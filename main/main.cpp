#include "main.h"

static const char *TAG { "main" };
static char *ssid, *wifi_pass, *mqtt_topic, *mqtt_broker, *timezone;
static TaskHandle_t main_task_handle;
static SemaphoreHandle_t backlog_semaphore, sensor_sleep_semaphore;
static esp_timer_handle_t timer;
static Sensor* sensors[] { new PMS5003(), new BME280() };

static void set_log_levels() {
	esp_log_level_set("*", ESP_LOG_NONE);
	esp_log_level_set("uart", ESP_LOG_WARN);
	esp_log_level_set("i2c", ESP_LOG_WARN);
	esp_log_level_set("sdcard", ESP_LOG_INFO);
	esp_log_level_set("ds3231", ESP_LOG_INFO);
	esp_log_level_set("main", ESP_LOG_DEBUG);
	esp_log_level_set("wlan", ESP_LOG_DEBUG);
	esp_log_level_set("mqtt", ESP_LOG_DEBUG);
	esp_log_level_set("http", ESP_LOG_INFO);
}

extern "C" void app_main() {
	// Install drivers and set logging
	initialize_required_services();

	// Mount the SD card - deny service until mounted
	if (!sdcard_mount(SDCARD_MOUNT_POINT)) {
		ESP_LOGW(TAG, "Insert SD card to continue...");
		do {
			vTaskDelay(2000 / portTICK_PERIOD_MS);
		} while (!sdcard_mount(SDCARD_MOUNT_POINT));
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
		cJSON *root { cJSON_Parse(file_str) }, *json;
		if (cJSON_IsInvalid(root)) {
			ESP_LOGE(TAG, "Unable to parse config file (invalid JSON)");
			ESP_LOGE(TAG, "Denying service...");
			vTaskDelay(portMAX_DELAY);
		}

		// TODO: Error checking for JSON object
		ssid = strdup(cJSON_GetObjectItem(root, "ssid")->valuestring);
		wifi_pass = strdup(cJSON_GetObjectItem(root, "pass")->valuestring);
		mqtt_topic = strdup(cJSON_GetObjectItem(root, "topic")->valuestring);
		mqtt_broker = strdup(cJSON_GetObjectItem(root, "mqtt")->valuestring);

		// Get the time zone string if it's present
		if (cJSON_IsString((json = cJSON_GetObjectItem(root, "tz")))) {
			ESP_LOGD(TAG, "Setting the time zone");
			timezone = strdup(json->valuestring);
		}

		// Set the log levels per the log JSON object
		json = cJSON_GetObjectItem(root, "log");
		if (cJSON_IsObject(json) && cJSON_GetArraySize(json) > 0) {
			const char *const levels[6] { "NONE", "ERROR", "WARN", "INFO",
				"DEBUG", "VERBOSE" };

			// Iterate through each item in the JSON object
			cJSON *elem;
			cJSON_ArrayForEach(elem, json) {
				char* key = elem->string;
				int value = elem->valueint;
				if (elem->valueint < 0 || elem->valueint > 5) {
					ESP_LOGE(TAG, "Unable to get log level for '%s'",
							elem->string);
				} else {
					ESP_LOGD(TAG, "Setting '%s' to ESP_LOG_%s", key,
							levels[value]);
					esp_log_level_set(key, static_cast<esp_log_level_t>(value));
				}
			}
		}

		cJSON_Delete(root);
	} else {
		ESP_LOGE(TAG, "Unable to load configuration file (cannot open file)");
		ESP_LOGE(TAG, "Denying service...");
		vTaskDelay(portMAX_DELAY);
	}

	// Set the system time if possible
	bool time_is_synchronized { false }, sntp_synchronized { false };
	if (!ds3231_lost_power() && esp_reset_reason() == ESP_RST_SW) {
		set_system_time(ds3231_get_time(), timezone);
		time_is_synchronized = true;
	}

	// Connect to WiFi, MQTT, and start the HTTP server
	wlan_connect(ssid, wifi_pass);
	mqtt_connect(mqtt_broker);
	http_start();

	// Synchronize system time with time server
	while (!time_is_synchronized) {
		if (!wlan_started()) vTaskDelay(portMAX_DELAY); // supress log spamming
		wlan_block_until_connected(5000); // block 5 seconds
		if (wlan_connected() && sntp_synchronize_system_time(timezone)) {
			time_is_synchronized = ds3231_set_time();
			sntp_synchronized = true;
		}
	}

	// Create sensor sleep task to sleep sensor after 1 second
	ESP_LOGD(TAG, "Starting sensor sleep task");
	sensor_sleep_semaphore = xSemaphoreCreateBinary();
	xTaskCreate(sensor_sleep_task, "sensor_sleep", 4096, nullptr, 1, nullptr);

	// Create the synchronize system time task
	ESP_LOGD(TAG, "Starting system time synchronization task");
	xTaskCreate(synchronize_system_time_task, "synchronize_system_time", 4096,
			&sntp_synchronized, 0, nullptr);

	// Create the send backlog task
	ESP_LOGD(TAG, "Starting data backlog monitor task");
	backlog_semaphore = xSemaphoreCreateBinary();
	xTaskCreate(send_backlogged_data_task, "send_backlogged_data", 4096,
			nullptr, 0, nullptr);

	// Check if there is data to be written
	ESP_LOGD(TAG, "Checking for backlogged data");
	if (access("/sdcard/data.txt", F_OK) != -1)
		xSemaphoreGive(backlog_semaphore);

	// Do initial sensor setup
	ESP_LOGI(TAG, "Setting up the sensors");
	for (Sensor *sensor : sensors)
		sensor->setup();

	// Calculate the next sensor ready time
	char ms_str[10];
	int64_t timer_wait_ms { set_window_wait_timer(timer, -(SENSOR_READY_MS + 1000)) };
	if (timer_wait_ms > 5000) xSemaphoreGive(sensor_sleep_semaphore);

	// Enter the task loop
	while (true) {
		ESP_LOGD(TAG, "Ready alarm will go off in %s", ms2str(ms_str, timer_wait_ms));
		xTaskNotifyWait(0, 0, 0, portMAX_DELAY);

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
		timer_wait_ms = set_window_wait_timer(timer, 0);
		ESP_LOGD(TAG, "Measure alarm will go off in %s", ms2str(ms_str, timer_wait_ms));
		xTaskNotifyWait(0, 0, 0, portMAX_DELAY);

		// Get the weather data
		ESP_LOGI(TAG, "Getting weather data");
		for (Sensor *sensor : sensors)
			sensor->get_data(json_data);
		xSemaphoreGive(sensor_sleep_semaphore);

		// Delete the JSON root
		ESP_LOGV(TAG, "Converting the JSON object to a string");
		char *json_str { cJSON_PrintUnformatted(json_root) };
		ESP_LOGV(TAG, "Deleting the JSON object");
		cJSON_Delete(json_root);

		// Handle the JSON string
		ESP_LOGI(TAG, "Publishing data to MQTT broker");
		if (!mqtt_publish(mqtt_topic, json_str)) {
			// Write the data to file
			ESP_LOGI(TAG, "Writing data to file");
			FILE *fd { fopen(DATA_FILE_PATH, "a+") };
			if (fd != nullptr) {
				if (fputs(json_str, fd) == -1 || fputc('\n', fd) == -1)
					ESP_LOGE(TAG, "Unable to write data to file (cannot write to file)");
				else xSemaphoreGive(backlog_semaphore);
				fclose(fd);
			} else {
				ESP_LOGE(TAG, "Unable to write data to file (cannot open file)");
			}
		}
		delete[] json_str;

		// Calculate next wake time
		timer_wait_ms = set_window_wait_timer(timer, -(SENSOR_READY_MS + 1000));
	}
}

void initialize_required_services() {
	// Setup log to card and initial log levels
	esp_log_set_vprintf(vlogf);
	set_log_levels();

	// Register shutdown handler
	ESP_LOGV(TAG, "Registering shutdown handler");
	auto shutdown_handler =  [] () {
		ESP_LOGI(TAG, "Restarting...");
		vTaskSuspend(main_task_handle);
		if (!http_stop()) ESP_LOGE(TAG, "Unable to stop the web server");
		if (!mqtt_stop()) ESP_LOGE(TAG, "Unable to stop the MQTT client");
		if (!wlan_stop()) ESP_LOGE(TAG, "Unable to stop the WiFi driver");
		if (!uart_stop()) ESP_LOGE(TAG, "Unable to stop the UART bus");
		if (!i2c_stop()) ESP_LOGE(TAG, "Unable to stop the I2C bus");
		if (!sdcard_unmount()) ESP_LOGE(TAG, "Unable to unmount the filesystem");
	};
	esp_register_shutdown_handler(shutdown_handler);

	esp_err_t setup_ret;
	ESP_LOGV(TAG, "Initializing NVS flash");
	if ((setup_ret = nvs_flash_init()) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to initialize NVS flash (%i)", setup_ret);
		abort();
	}

	ESP_LOGV(TAG, "Initializing net interface");
	if ((setup_ret = esp_netif_init()) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to initialize network interface (%i)", setup_ret);
		abort();
	}

	ESP_LOGV(TAG, "Creating default event loop");
	if ((setup_ret = esp_event_loop_create_default()) != ESP_OK) {
		ESP_LOGE(TAG, "Unable to create default event loop (%i)", setup_ret);
		abort();
	}

	ESP_LOGV(TAG, "Starting the UART port");
	if (!uart_start()) {
		ESP_LOGE(TAG, "Unable to start the UART port");
		abort();
	}

	ESP_LOGV(TAG, "Starting the I2C port");
	if (!i2c_start()) {
		ESP_LOGE(TAG, "Unable to start the I2C port");
		abort();
	}

	// Get the handle for the main task
	main_task_handle = xTaskGetCurrentTaskHandle();

	// Set main task to high priority
	const int priority { 10 };
	ESP_LOGV(TAG, "Setting main task to priority %i", priority);
	vTaskPrioritySet(main_task_handle, priority);
	if (static_cast<int>(uxTaskPriorityGet(nullptr)) != priority) {
		ESP_LOGE(TAG, "Unable to set main task to priority %i", priority);
		abort();
	}

	// Configure the main timer
	ESP_LOGV(TAG, "Configuring the main timer");
	auto timer_callback = [] (void *args) {
		xTaskNotifyGive(main_task_handle);
	};
	esp_timer_create_args_t timer_args;
	timer_args.callback = timer_callback;
	timer_args.name = "main_timer";
	esp_timer_create(&timer_args, &timer);

}

int vlogf(const char *format, va_list arg) {
	// Get an appropriate sized buffer for the message
	const int len { vsnprintf(nullptr, 0, format, arg) };
	char message[len + 1];
	vsprintf(message, format, arg);

	// Open the file and delete it if it gets too big
	FILE *fd { fopen(LOG_FILE_PATH, "a+") };
	if (fd != nullptr) {
		// Delete the log file if it gets too big
		if (fsize(fd) > LOG_FILE_MAX_SIZE_BYTES) {
			// TODO: replace with freopen(LOG_FILE_PATH, "w", fd) ?
			fclose(fd);
			remove(LOG_FILE_PATH);
			fd = fopen(LOG_FILE_PATH, "a+");
		}

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

void synchronize_system_time_task(void *args) {
	// Check if we should skip the initial synchronization
	bool time_is_synchronized { *static_cast<bool*>(args) };
	if (time_is_synchronized)
		ESP_LOGD(TAG, "Skipping initial SNTP server synchronization (already synchronized)");
	else wlan_block_until_connected(); // suppress first warning
	while (true) {
		while (!time_is_synchronized) {
			if (wlan_connected()) {
				time_is_synchronized = sntp_synchronize_system_time(timezone);
				if (time_is_synchronized) ds3231_set_time();
			} else {
				ESP_LOGW(TAG, "Unable to synchronize system time (not connected)");
				vTaskDelay((60 * 1000) / portTICK_PERIOD_MS); // 1 minute
			}
		};
		vTaskDelay((7 * 24 * 60 * 60 * 1000) / portTICK_PERIOD_MS); // 1 week
		time_is_synchronized = false;
	}
}

void send_backlogged_data_task(void *args) {
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
				int c, line_len = 1;
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

void sensor_sleep_task(void *args) {
	while (true) {
		// Wait one second then sleep
		if (xSemaphoreTake(sensor_sleep_semaphore, portMAX_DELAY) == pdTRUE) {
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			ESP_LOGI(TAG, "Putting sensors to sleep");
			for (Sensor *sensor : sensors)
				sensor->sleep();
		}
	}
}
