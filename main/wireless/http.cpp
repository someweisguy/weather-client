/*
 * http.cpp
 *
 *  Created on: May 10, 2020
 *      Author: Mitch
 */

#include "http.h"
static const char *TAG { "http" };
static httpd_handle_t server { nullptr };

static esp_err_t http_restart_handler(httpd_req_t *r) {
	ESP_LOGD(TAG, "Handling restart request");
	ESP_LOGV(TAG, "Responding to client");
	esp_err_t resp_ret { httpd_resp_sendstr(r, "OK") };
	if (resp_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
		return ESP_FAIL;
	}

	/**
	 * The ESP-IDF does not have an event loop for HTTP server events (though
	 * it does have one for the http client) so what we do is we pretend to
	 * give the server a session context, which means when the session closes,
	 * it will call the function to free the context. Instead of freeing
	 * context, what it will do is call the esp_restart(). This way, the server
	 * waits for the response to be sent before it calls the restart function.
	 */
	auto restart_callback = [] (void *ctx) {
	    const auto restart_task = [] (void *args) { esp_restart(); };
	    // Run the code in a separate task or http_stop() hangs
	    xTaskCreate(restart_task, "restart", 2048, nullptr, 0, nullptr);
	};

	r->sess_ctx = &server; // give the session fake context to free
	r->free_ctx = restart_callback;

	return ESP_OK;
}

static esp_err_t http_set_log_level_handler(httpd_req_t *r) {
	ESP_LOGD(TAG, "Handling set log level request");

	// FIXME: Corrupt heap when adding new entries
	// FIXME: Null pointer (I think) when deleting nonexistent entries

	// Read the content into a buffer
	ESP_LOGV(TAG, "Reading data from client");
	char wjson_str[r->content_len];
	int read = httpd_req_recv(r, wjson_str, r->content_len);
	if (read >= 0) wjson_str[read] = 0; // append null terminator
	else ESP_LOGE(TAG, "Unable to read data from client (%i)", read);
	ESP_LOGV(TAG, "Got %i bytes: %s", r->content_len, wjson_str);

	// Check if the content is a valid JSON object
	ESP_LOGV(TAG, "Checking if client data is valid JSON");
	cJSON *wroot { cJSON_Parse(wjson_str) };
	if (cJSON_IsInvalid(wroot)) {
		ESP_LOGW(TAG, "Client data is invalid JSON");
		ESP_LOGV(TAG, "Responding to client");
		esp_err_t resp_ret { httpd_resp_sendstr(r, "FAIL") };
		if (resp_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
			return ESP_FAIL;
		}
	}

	// Read the file JSON object into memory
	ESP_LOGV(TAG, "Reading config file into memory");
	cJSON *froot;
	FILE *fd { fopen(CONFIG_FILE_PATH, "r") };
	if (fd != nullptr) {
		// Read the JSON file into memory
		ESP_LOGV(TAG, "Reading config file into memory");
		const long size { fsize(fd) };
		if (size > 1024)
			ESP_LOGW(TAG, "Config file is larger than expected (%li bytes)", size);
		char file_str[size + 1];
		fread(file_str, 1, size, fd);
		// don't call fclose()

		// Parse the file JSON object
		ESP_LOGV(TAG, "Parsing JSON file");
		froot = cJSON_Parse(file_str);
	} else {
		ESP_LOGE(TAG, "Unable to open config file");
		ESP_LOGV(TAG, "Responding to client");
		esp_err_t resp_ret { httpd_resp_sendstr(r, "FAIL") };
		if (resp_ret != ESP_OK) {
			ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
			return ESP_FAIL;
		} else return ESP_OK;
	}

	// Get the JSON log item or create it if it doesn't exist
	cJSON *log, *i;
	if ((log = cJSON_GetObjectItem(froot, "log")) == nullptr) {
		ESP_LOGV(TAG, "Creating a JSON log object");
		cJSON_AddItemToObject(froot, "log", log=cJSON_CreateObject());
	}

	// Add each web JSON object to the file JSON object and set log level
	ESP_LOGV(TAG, "Attaching client data JSON to config file JSON");
	cJSON_ArrayForEach(i, wroot) {

		// Check if the element is a duplicate
		cJSON *j;
		bool is_duplicate { false };
		int idx { 0 };
		cJSON_ArrayForEach(j, log) {
			if (strcmp(i->string, j->string) == 0) {
				j->valueint = i->valueint;
				is_duplicate = true;
				break;
			}
			++idx;
		}

		// Delete item if there log level not in bounds
		if (is_duplicate) {
			if (i->valueint < 0 || i->valueint > 5) {
				ESP_LOGD(TAG, "Deleting '%s' from log configuration", i->string);
				cJSON_DeleteItemFromArray(log, idx);
				i->valueint = 0; // ESP_LOG_NONE
			} else {
				ESP_LOGD(TAG, "Overwriting '%s' in log configuration", i->string);
			}
		} else {
			cJSON_AddItemToArray(log, i);
		}

		// Set the log level
		const char* key = i->string;
		const int value = i->valueint;
		const char *const s[6] { "NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE" };
		ESP_LOGI(TAG, "Setting '%s' to log level ESP_LOG_%s", key, s[value]);
		esp_log_level_set(key, static_cast<esp_log_level_t>(value));
	}

	// Print the edited JSON object to file
	ESP_LOGV(TAG, "Writing new JSON object to file");
	char* fjson_str = cJSON_Print(froot);

	// Clear and write to the config file
	ESP_LOGV(TAG, "Reopening config file");
	fd = freopen(CONFIG_FILE_PATH, "w", fd);
	if (fd != nullptr) {
		fputs(fjson_str, fd);
		fclose(fd);
		vPortYield(); // stream write before freeing string
	} else {
		ESP_LOGE(TAG, "Unable to write new JSON object to file");
	}

	// Clean up
	cJSON_Delete(wroot);
	cJSON_Delete(froot);
	delete[] fjson_str;

	// Send a response to the client
	ESP_LOGV(TAG, "Responding to client");
	esp_err_t resp_ret { httpd_resp_sendstr(r, "OK") };
	if (resp_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
		return ESP_FAIL;
	}
	return ESP_OK;
}

static esp_err_t http_get_log_level_handler(httpd_req_t *r) {
	// TODO: get log JSON and send it to client
	return ESP_OK;
}

static esp_err_t http_get_log_events_handler(httpd_req_t *r) {
	// TODO: send the log to the client
	// Maybe in the post data, specify how many of the last lines you want?
	return ESP_OK;
}

bool http_start() {
	if (server != nullptr)
		return true;

	// Configure and start the web server
	ESP_LOGD(TAG, "Starting the web server");
    const httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    const esp_err_t start_ret { httpd_start(&server, &config) };
    if (start_ret != ESP_OK) {
    	ESP_LOGE(TAG, "Unable to start the web server");
    	return false;
    }

    // Register restart handler
    ESP_LOGD(TAG, "Registering restart handler");
	httpd_uri_t restart_uri;
	restart_uri.method = HTTP_POST;
	restart_uri.uri = "/restart";
	restart_uri.handler = http_restart_handler;
	esp_err_t ret = httpd_register_uri_handler(server, &restart_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register restart handler (%i)", ret);

	// Register set log level handler
	ESP_LOGD(TAG, "Registering set log level handler");
	httpd_uri_t set_level_uri;
	set_level_uri.method = HTTP_POST;
	set_level_uri.uri = "/set_level";
	set_level_uri.handler = http_set_log_level_handler;
	ret = httpd_register_uri_handler(server, &set_level_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register set log level handler (%i)", ret);

	// Register get log level handler
	ESP_LOGD(TAG, "Registering get log level handler");
	httpd_uri_t get_level_uri;
	get_level_uri.method = HTTP_POST;
	get_level_uri.uri = "/get_level";
	get_level_uri.handler = http_get_log_level_handler;
	ret = httpd_register_uri_handler(server, &get_level_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register get log level handler (%i)", ret);

	// Register get log level handler
	ESP_LOGD(TAG, "Registering get log events handler");
	httpd_uri_t get_events_uri;
	get_events_uri.method = HTTP_POST;
	get_events_uri.uri = "/get_events";
	get_events_uri.handler = http_get_log_events_handler;
	ret = httpd_register_uri_handler(server, &get_events_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register get log events handler (%i)", ret);

    return true;
}

bool http_stop() {
	if (server == nullptr)
		return true;

	// Stop the web server
	ESP_LOGD(TAG, "Stopping the web server");
	esp_err_t stop_ret { httpd_stop(server) };
	if (stop_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to stop the web server");
		return false;
	}

	server = nullptr;
	return true;
}

