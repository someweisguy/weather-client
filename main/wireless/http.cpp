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

	// Read the content into a buffer
	ESP_LOGV(TAG, "Getting data from client");
	char web_json_str[r->content_len];
	int read = httpd_req_recv(r, web_json_str, r->content_len);
	if (read >= 0) {
		web_json_str[read] = 0; // add null terminator
		ESP_LOGV(TAG, "Got %i bytes from client: %s", r->content_len, web_json_str);
	} else {
		ESP_LOGE(TAG, "Unable to get data from client (%i)", read);
		httpd_resp_set_status(r, "400 Bad Request");
		esp_err_t resp_ret { httpd_resp_send(r, "", 0) };
		if (resp_ret != ESP_OK)
			ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
		return resp_ret;
	}

	// Check if the content is a valid JSON object
	ESP_LOGV(TAG, "Checking if client data is valid JSON");
	cJSON *web_root { cJSON_Parse(web_json_str) };
	if (cJSON_IsInvalid(web_root)) {
		ESP_LOGW(TAG, "Client data is invalid JSON");
		ESP_LOGV(TAG, "Responding to client");
		httpd_resp_set_status(r, "400 Bad Request");
		esp_err_t resp_ret { httpd_resp_send(r, "", 0) };
		if (resp_ret != ESP_OK)
			ESP_LOGE(TAG, "Unable to respond to the client (%i)", resp_ret);
		return resp_ret;
	}

	// Read the file JSON object into memory
	ESP_LOGV(TAG, "Reading the configuration file into memory");
	cJSON *file_root;
	FILE *fd { fopen(CONFIG_FILE_PATH, "r") };
	if (fd != nullptr) {
		const long size { fsize(fd) };
		if (size > 1024)
			ESP_LOGW(TAG, "Config file is larger than expected (%li bytes)", size);
		char file_str[size + 1];
		fread(file_str, 1, size, fd);
		// don't call fclose() - calling freopen() later
		file_root = cJSON_Parse(file_str);
	} else {
		ESP_LOGE(TAG, "Unable to open the configuration file");
		ESP_LOGV(TAG, "Responding to client");
		httpd_resp_set_status(r, "500 Internal Server Error");
		esp_err_t resp_ret { httpd_resp_send(r, "", 0) };
		if (resp_ret != ESP_OK)
			ESP_LOGE(TAG, "Unable to respond to the client (%i)", resp_ret);
		cJSON_Delete(web_root);
		return resp_ret;
	}

	// Get the JSON log item or create it if it doesn't exist
	cJSON *log, *web_item;
	if ((log = cJSON_GetObjectItem(file_root, "log")) == nullptr) {
		ESP_LOGV(TAG, "Creating a new JSON log object");
		cJSON_AddItemToObject(file_root, "log", log=cJSON_CreateObject());
	}

	// Add each web JSON object to the file JSON object and set log level
	ESP_LOGV(TAG, "Attaching client data JSON to config file JSON");
	cJSON_ArrayForEach(web_item, web_root) {
		const char* name = web_item->string;
		const int value = web_item->valueint;

		// Check if the element is a duplicate
		cJSON *log_item;
		bool is_duplicate { false };
		int duplicate_index { 0 };
		cJSON_ArrayForEach(log_item, log) {
			const char *log_item_name { log_item->string };
			if (strcmp(name, log_item_name) == 0) {
				log_item->valueint = value;
				is_duplicate = true;
				break;
			}
			++duplicate_index;
		}

		// Add or delete the item from the log file JSON object
		if (is_duplicate && (value < 0 || value > 5)) {
			ESP_LOGD(TAG, "Deleting '%s' from the log configuration file", name);
			cJSON_DeleteItemFromArray(log, duplicate_index);
			esp_log_level_set(name, ESP_LOG_NONE); // silence removed items
		} else if (!is_duplicate) {
			if (value >= 0 && value <= 5) {
				if (strlen(name) > 0) cJSON_AddNumberToObject(log, name, value);
			} else
				ESP_LOGW(TAG, "Unable to delete entry for '%s' (no such entry)",
						name);
		}
	}
	cJSON_Delete(web_root); // done with web JSON

	// Print the edited JSON object to file
	ESP_LOGV(TAG, "Writing new JSON object to file");
	fd = freopen(CONFIG_FILE_PATH, "w", fd);
	if (fd != nullptr) {
		char* file_json_str = cJSON_Print(file_root);
		if (fputs(file_json_str, fd) == EOF) {
			ESP_LOGE(TAG, "Unable to write the new JSON object to file (cannot write)");
			httpd_resp_set_status(r, "500 Internal Server Error");
		} else {
			const char *const l[6] { "NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE" };
			cJSON *i;
			cJSON_ArrayForEach(i, log) {
				// Set the log level
				const char* name = i->string;
				const int value = i->valueint;
				ESP_LOGI(TAG, "Setting '%s' to log level ESP_LOG_%s", name, l[value]);
				esp_log_level_set(name, static_cast<esp_log_level_t>(value));
			}
		}
		delete[] file_json_str;
		fclose(fd);
	} else {
		ESP_LOGE(TAG, "Unable to write the new JSON object to file (cannot open file)");
		httpd_resp_set_status(r, "500 Internal Server Error");
	}
	cJSON_Delete(file_root); // done with file JSON

	// Send a response to the client
	ESP_LOGV(TAG, "Responding to client");
	httpd_resp_set_status(r, "204 No Content");
	esp_err_t resp_ret { httpd_resp_send(r, "", 0) };
	if (resp_ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
	return resp_ret;
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

