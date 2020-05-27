/*
 * http.cpp
 *
 *  Created on: May 10, 2020
 *      Author: Mitch
 */

#include "http.h"
static const char *TAG { "http" };
static httpd_handle_t server { nullptr };

static esp_err_t http_load_config(httpd_req_t *r, cJSON *&root, char *&response) {
	// Read the file into memory
	ESP_LOGV(TAG, "Reading the configuration file into memory");
	const esp_err_t load_ret { get_config_resource(root) };
	switch (load_ret) {
	case ESP_OK:
		return ESP_OK;

	case ESP_ERR_NOT_FOUND:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("Unable to open the configuration file");
		ESP_LOGE(TAG, "%s", response);
		return ESP_FAIL;

	case ESP_ERR_INVALID_SIZE: {
		const char* warn_fmt { "299 " USER_AGENT "%s \"config file too big\"" };
		const char* version { esp_get_idf_version() };
		char warning[strlen(warn_fmt) + strlen(version) + 1];
		sprintf(warning, warn_fmt, version);
		httpd_resp_set_hdr(r, "Warning", warning);
		response = strdup("Configuration file is larger than expected");
		ESP_LOGW(TAG, "%s", response);
		return ESP_OK; // OK
	}

	case ESP_ERR_NO_MEM:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("Unable to allocate memory to read configuration file");
		ESP_LOGE(TAG, "%s", response);
		return ESP_FAIL;

	case ESP_ERR_INVALID_STATE:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("Configuration file is invalid JSON");
		ESP_LOGE(TAG, "%s", response);
		return ESP_FAIL;

	case ESP_FAIL:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("Unable to write to the configuration file");
		ESP_LOGE(TAG, "%s", response);
		return ESP_FAIL;

	default:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("An unexpected error occurred (check log)");
		ESP_LOGE(TAG, "Unexpected error in load_config_json (%i)", load_ret);
		return ESP_FAIL;
	}
}

static esp_err_t http_write_config(httpd_req_t *r, cJSON *root, char *&response) {
	// Write the JSON object into memory
	ESP_LOGV(TAG, "Writing the configuration file into memory");
	const esp_err_t write_ret { set_config_resource(root) };
	switch (write_ret) {
	case ESP_OK:
		return ESP_OK;

	case ESP_ERR_INVALID_SIZE:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("Configuration file is too big");
		ESP_LOGE(TAG, "%s", response);
		return ESP_FAIL;

	case ESP_FAIL:
	case ESP_ERR_NOT_FOUND:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("Unable to write to the configuration file");
		ESP_LOGE(TAG, "%s", response);
		ESP_LOGE(TAG, "Error code %i", write_ret);
		return ESP_FAIL;

	default:
		httpd_resp_set_status(r, HTTPD_500);
		response = strdup("An unexpected error occurred (check log)");
		ESP_LOGE(TAG, "Unexpected error in load_config_json (%i)", write_ret);
		return ESP_FAIL;
	}
}

static int get_query(httpd_req_t *r, char *query_buffer) {
	const size_t len { httpd_req_get_url_query_len(r) + 1};
	if (len == 0) return 0;
	if (query_buffer != nullptr) {
		esp_err_t ret { httpd_req_get_url_query_str(r, query_buffer, len) };
		if (ret != ESP_OK) return -1;
	}
	return len;
}

static char **get_query_keys(const char *query) {
	// Count the number of keys and allocate memory
	int num_keys { 1 };
	for (int i = 0; query[i]; ++i)
		if (query[i] == '&') ++num_keys;
	char **keys = new char*[num_keys + 1];
	keys[num_keys] = nullptr; // null terminator
	ESP_LOGV(TAG, "Got %i keys", num_keys);

	// Parse the query to extract all the keys
	bool in_key { true };
	for (int i = 0, s = 0, n = 0; query[i]; ++i) {
		// Read the end of the key?
		if (in_key && query[i] == '=') {
			// Allocate memory and read the key
			const int key_len { i - s };
			keys[n] = new char[key_len + 1];
			for (int j = 0; j < key_len; ++j)
				keys[n][j] = query[s + j];
			keys[n++][key_len] = 0;

			in_key = false;
		} else if (!in_key && query[i] == '&') {
			s = i + 1;
			in_key = true;
		}
	}

	return keys;
}

static esp_err_t http_send_response(httpd_req_t *r, const char *body) {
	ESP_LOGV(TAG, "Responding to client");
	esp_err_t resp_ret { httpd_resp_sendstr(r, body) };
	if (resp_ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
	return resp_ret;
}

static esp_err_t http_restart_handler(httpd_req_t *r) {
	ESP_LOGD(TAG, "Handling restart request");
	ESP_LOGV(TAG, "Responding to client");
	http_send_response(r, "");

	// TODO: send HTTP accepted

	/**
	 * The ESP-IDF does not have an event loop for HTTP server events (though
	 * it does have one for the http client) so what we do is we pretend to
	 * give the server a session context, which means when the session closes,
	 * it will call a function to free the context. Instead of freeing context,
	 * what it will do is call the esp_restart(). This way, the server waits
	 * for the response to be sent before it calls the restart function.
	 */
	auto restart_callback = [] (void *ctx) {
	    const auto restart_task = [] (void *args) { esp_restart(); };
	    // Call the restart from a separate task or http_stop() hangs
	    xTaskCreate(restart_task, "restart", 2048, nullptr, 0, nullptr);
	};
	r->sess_ctx = reinterpret_cast<void*>(-1);
	r->free_ctx = restart_callback;

	return ESP_OK;
}

static esp_err_t http_get_logging(httpd_req_t *r) {
	ESP_LOGV(TAG, "Handling GET /logging");

	// Load the JSON root from file
	cJSON *root; // free only on success
	char *response; // free only on failure
	esp_err_t ret { http_load_config(r, root, response) };
	if (ret != ESP_OK) {
		ret = http_send_response(r, response);
		delete[] response;
		return ret;
	}

	// Get the JSON log item
	cJSON *log = cJSON_GetObjectItem(root, "log");
	if (log == nullptr) {
		httpd_resp_set_status(r, HTTPD_204);
		ret = http_send_response(r, nullptr);
	} else {
		// Get the content
		const char *json_str { cJSON_PrintUnformatted(log) };

		// Set the status code and type header
		httpd_resp_set_status(r, HTTPD_200);
		httpd_resp_set_type(r, "application/json");

		// Send the response and free the content
		ret = http_send_response(r, json_str);
		delete[] json_str;
	}

	cJSON_Delete(root);
	return ret;
}

static esp_err_t http_put_logging(httpd_req_t *r) {
	ESP_LOGD(TAG, "Handling PUT /logging");

	// Load the JSON root from file
	cJSON *root; // free only on success
	char *response; // free only on failure
	esp_err_t ret { http_load_config(r, root, response) };
	if (ret != ESP_OK) {
		ret = http_send_response(r, response);
		delete[] response;
		return ret;
	}

	// Get the URL query from the client
	int query_len { get_query(r, nullptr) };
	char query[query_len + 1];
	get_query(r, query);

	// Get the queries keys and the number of keys
	int num_keys { 0 };
	char **keys { get_query_keys(query) };
	while (keys[num_keys]) ++num_keys;

	// Get or create the JSON log object
	cJSON *log { cJSON_GetObjectItem(root, "log") };
	if (log == nullptr) {
		ESP_LOGV(TAG, "Creating a new JSON log object");
		cJSON_AddItemToObject(root, "log", log=cJSON_CreateObject());
	}

	// Iterate the keys and add each key value to the JSON log object
	cJSON *deleted { nullptr };
	char **warnings { new char*[num_keys] }; // must be heap
	for (int i = 0; keys[i] != nullptr; ++i) {
		char *key { keys[i] };

		// Get the key value
		char val_buf[3];
		int level { 6 }; // assume error
		ret = httpd_query_key_value(query, key, val_buf, 3);
		if (ret == ESP_OK) sscanf(val_buf, "%i", &level);

		// Add HTTP header warning on failure to parse value
		if (ret != ESP_OK || level > 5) {
			ESP_LOGW(TAG, "Unable parse key '%s' (%i)", key, ret);
			const char* warn_fmt { "299 " USER_AGENT "%s \"parse error: %s\"" };
			const char* version { esp_get_idf_version() };
			warnings[i] = new char [strlen(warn_fmt) + strlen(version) + strlen(key) + 1];
			sprintf(warnings[i], warn_fmt, version, key);
			ESP_LOGV(TAG, "Sending warning: %s", warnings[i]);
			httpd_resp_set_hdr(r, "Warning", warnings[i]);
			continue;
		}

		// Add the key and value pair to the JSON log object
		cJSON *item { cJSON_GetObjectItem(log, key) };
		if (level >= 0) {
			if (item == nullptr) {
				ESP_LOGV(TAG, "Creating new item '%s'", key);
				httpd_resp_set_status(r, HTTPD_201);
				cJSON_AddItemToObject(log, key, cJSON_CreateNumber(level));
			} else {
				httpd_resp_set_status(r, HTTPD_200);
				ESP_LOGV(TAG, "Found existing item '%s'", key);
				item->valueint = level;
			}
		} else {
			if (item == nullptr) {
				httpd_resp_set_status(r, HTTPD_204);
				// Silently set the log level to ESP_LOG_NONE
				esp_log_level_set(key, ESP_LOG_NONE);
			} else {
				httpd_resp_set_status(r, HTTPD_200);
				ESP_LOGV(TAG, "Deleting existing item '%s'", key);
				cJSON *det { cJSON_DetachItemFromObject(log, key) };
				if (deleted == nullptr) deleted = cJSON_CreateArray();
				cJSON_AddItemToArray(deleted, det);
			}
		}
	}

	// Write the JSON root object back to disk
	ret = http_write_config(r, root, response);
	if (ret != ESP_OK) {
		ret = http_send_response(r, response);
		delete[] response;
		cJSON_Delete(root);
		delete[] keys;
		delete[] warnings;
		return ret;
	}

	// Iterate the JSON log object and set log levels - just the new items
	for (int i = 0; keys[i] != nullptr; ++i) {
		char *key { keys[i] };
		const char *l[6] { "NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE" };
		cJSON *item { cJSON_GetObjectItem(log, key) };
		if (item != nullptr) {
			const int level { item->valueint };
			ESP_LOGI(TAG, "Setting '%s' to ESP_LOG_%s", key, l[level]);
			esp_log_level_set(key, static_cast<esp_log_level_t>(level));
		}
	}

	// Iterate through the deleted items and set them to log none
	cJSON *item;
	cJSON_ArrayForEach(item, deleted) {
		ESP_LOGI(TAG, "Setting '%s' to ESP_LOG_NONE", item->string);
		esp_log_level_set(item->string, ESP_LOG_NONE);
	}
	cJSON_Delete(deleted);

	// Cleanup and send response
	delete[] keys;
	delete[] warnings;
	cJSON_Delete(root);
	return http_send_response(r, "");
}

static esp_err_t http_head_events(httpd_req_t *r) {
	// Open the log file
	FILE *fd { fopen(LOG_FILE, "r") };
	if (fd == nullptr) {
		httpd_resp_set_status(r, HTTPD_500);
		esp_err_t ret { http_send_response(r, "") };
		return ret;
	}

	// Get the file size
	long file_size { fsize(fd) };
	fclose(fd);

	// Create a custom HTTP header
	const char *hdr_fmt {
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: %ld\r\n"
		"Content-Type: text/plain\r\n"
		};
	const int size_len { snprintf(nullptr, 0, "%li", file_size) };
	const size_t buf_len { strlen(hdr_fmt) + size_len };
	char hdr[buf_len + 1];
	sprintf(hdr, hdr_fmt, file_size);

	// Send the header using the raw HTTP send function
	ESP_LOGV(TAG, "Responding to client");
	esp_err_t resp_ret { httpd_send(r, hdr, buf_len) };
	if (resp_ret < 0)
		ESP_LOGE(TAG, "Unable to respond to client (%i)", resp_ret);
	return resp_ret;
}

static esp_err_t http_get_events(httpd_req_t *r) {

	// Open the log file
	FILE *fd { fopen(LOG_FILE, "r") };
	if (fd == nullptr) {
		httpd_resp_set_status(r, HTTPD_500);
		esp_err_t ret { http_send_response(r, "") };
		return ret;
	}

	// Get the URL query from the client
	int query_len { get_query(r, nullptr) };
	char query[query_len + 1];
	get_query(r, query);

	// Get the queries keys and the number of keys
	int num_keys { 0 };
	char **keys { get_query_keys(query) };
	while (keys[num_keys]) ++num_keys;
	// TODO: parse keys - including the requested size
	delete[] keys;

	// Ensure that we aren't sending more data than the file contains
	const long csize { fsize(fd) }, rsize { LONG_MAX };
	const long send_size { csize > rsize ? rsize : csize };

	// TODO: Seek by lines?
	// Seek to the closest newline to the requested size
	if (send_size != -1) {
		fseek(fd, -send_size, SEEK_END);
		for (int c = fgetc(fd); c != '\n' && c != EOF; c = fgetc(fd));
	}

	// Allocate the smallest chunk size possible
	const size_t heap { heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) };
	size_t chunk_size { MAX_CHUNK_SIZE > heap ? heap : MAX_CHUNK_SIZE };
	chunk_size = chunk_size > send_size ? send_size : chunk_size;
	char *chunk { new char[chunk_size] };

	// Send the log file in chunks
	httpd_resp_set_type(r, "text/plain");
	const int size_len { snprintf(nullptr, 0, "%li", send_size) };
	char content_len[size_len];
	snprintf(content_len, size_len, "%li", send_size);
	httpd_resp_set_hdr(r, "Content-Length", content_len);
	size_t chunk_read;
	do {
		chunk_read = fread(chunk, 1, chunk_size, fd);
		httpd_resp_send_chunk(r, chunk, chunk_read);
	} while (chunk_read > 0);
	httpd_resp_send_chunk(r, chunk, 0);
	delete[] chunk;
	fclose(fd);

	return ESP_OK;
}

static esp_err_t http_get_tz(httpd_req_t *r) {
	ESP_LOGV(TAG, "Handling GET /tz request");

	// Load the JSON root from file
	cJSON *root; // free only on success
	char *response; // free only on failure
	esp_err_t ret { http_load_config(r, root, response) };
	if (ret != ESP_OK) {
		ret = http_send_response(r, response);
		delete[] response;
		return ret;
	}

	// Get the tz JSON object if it exists
	cJSON *tz { cJSON_GetObjectItem(root, "tz") };
	if (tz == nullptr) {
		httpd_resp_set_status(r, HTTPD_204);
		ret = http_send_response(r, "");
	} else {
		httpd_resp_set_status(r, HTTPD_200);
		ret = http_send_response(r, tz->valuestring);
	}

	cJSON_Delete(root);
	return ret;
}

static esp_err_t http_put_tz(httpd_req_t *r) {
	ESP_LOGD(TAG, "Handling set timezone request");

	// Read the content into a buffer
	ESP_LOGV(TAG, "Getting data from client");
	char web_json_str[r->content_len];
	int read = httpd_req_recv(r, web_json_str, r->content_len);
	if (read >= 0) {
		web_json_str[read] = 0; // add null terminator
		ESP_LOGV(TAG, "Got %i bytes from client: %s", r->content_len, web_json_str);
	} else {
		const char* resp { "Unable to get data from client" };
		ESP_LOGE(TAG, "%s", resp);
		return http_send_response(r, resp);
	}

	// Check if the content is a valid JSON object
	ESP_LOGV(TAG, "Checking if client data is valid JSON");
	cJSON *web_root { cJSON_Parse(web_json_str) }, *web_tz;
	if (cJSON_IsInvalid(web_root)) {
		const char *resp { "Client data is invalid JSON" };
		ESP_LOGW(TAG, "%s", resp);
		return http_send_response(r, resp);
	} else if ((web_tz = cJSON_GetObjectItem(web_root, "tz")) == nullptr) {
		const char *resp { "Client data does not specify tz info" };
		ESP_LOGW(TAG, "%s", resp);
		return http_send_response(r, resp);
	}

	// Copy the web tz string into a buffer and delete the web JSON root
	const size_t len { strlen(web_tz->valuestring) };
	char tz_str[len + 1];
	strcpy(tz_str, web_tz->valuestring);
	cJSON_Delete(web_root);

	// Read the file JSON object into memory
	ESP_LOGV(TAG, "Reading the configuration file into memory");
	cJSON *file_root;
	FILE *fd { fopen(CONFIG_FILE, "r") };
	if (fd != nullptr) {
		const long size { fsize(fd) };
		if (size > 1024)
			ESP_LOGW(TAG, "Config file is larger than expected (%li bytes)", size);
		char file_str[size + 1];
		fread(file_str, 1, size, fd);
		fclose(fd);
		file_root = cJSON_Parse(file_str);
	} else {
		const char *resp { "Unable to open the configuration file" };
		ESP_LOGE(TAG, "%s", resp);
		cJSON_Delete(web_root);
		return http_send_response(r, resp);
	}

	// Get the JSON log item or create it if it doesn't exist
	cJSON *file_tz = cJSON_CreateString(tz_str);
	if (cJSON_GetObjectItem(file_root, "tz") == nullptr) {
		ESP_LOGV(TAG, "Creating a new JSON tz object");
		cJSON_AddItemToObject(file_root, "tz", file_tz);
	} else {
		cJSON_ReplaceItemInObject(file_root, "tz", file_tz);
	}

	// Print the edited JSON object to file
	ESP_LOGV(TAG, "Writing new JSON object to file");
	fd = fopen(CONFIG_FILE, "w");
	if (fd != nullptr) {
		char* file_json_str = cJSON_Print(file_root);
		if (fputs(file_json_str, fd) == EOF) {
			const char* resp { "Unable to write the new JSON object to file (cannot write)" };
			ESP_LOGE(TAG, "%s", resp);
			fclose(fd);
			delete[] file_json_str;
			cJSON_Delete(file_root);
			return http_send_response(r, resp);
		} else {
			setenv("TZ", tz_str, 1);
			tzset();
			delete[] file_json_str;
		}
		fclose(fd);
	} else {
		const char *resp { "Unable to write the new JSON object to file (cannot open file)" };
		ESP_LOGE(TAG, "%s", resp);
		cJSON_Delete(file_root);
		return httpd_resp_set_status(r, "500 Internal Server Error");
	}
	cJSON_Delete(file_root); // done with file JSON object

	// Send a response to the client
	return http_send_response(r,"OK");

	return ESP_OK;
}

static esp_err_t http_get_mqtt_handler(httpd_req_t *r) {
	// TODO
	return ESP_OK;
}

static esp_err_t http_set_mqtt_handler(httpd_req_t *r) {
	// TODO
	return ESP_OK;
}

bool http_start() {
	if (server != nullptr)
		return true;

	// Configure and start the web server
	ESP_LOGD(TAG, "Starting the web server");
    const httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t ret { httpd_start(&server, &config) };
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Unable to start the web server");
    	return false;
    }

	// Register get logging handler
	ESP_LOGV(TAG, "Registering GET /logging handler");
	httpd_uri_t get_logging_uri;
	get_logging_uri.method = HTTP_GET;
	get_logging_uri.uri = "/logging";
	get_logging_uri.handler = http_get_logging;
	ret = httpd_register_uri_handler(server, &get_logging_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register GET /logging handler (%i)", ret);

	// Register set logging handler
	ESP_LOGV(TAG, "Registering PUT /logging handler");
	httpd_uri_t put_logging_uri;
	put_logging_uri.method = HTTP_PUT;
	put_logging_uri.uri = "/logging";
	put_logging_uri.handler = http_put_logging;
	ret = httpd_register_uri_handler(server, &put_logging_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register PUT /logging handler (%i)", ret);

	// TODO: register delete logging handler

	// Register get events handler
	ESP_LOGV(TAG, "Registering GET /events handler");
	httpd_uri_t get_events_uri;
	get_events_uri.method = HTTP_GET;
	get_events_uri.uri = "/events";
	get_events_uri.handler = http_get_events;
	ret = httpd_register_uri_handler(server, &get_events_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register GET /events handler (%i)", ret);

	// Register head events handler
	ESP_LOGV(TAG, "Registering HEAD /events handler");
	httpd_uri_t head_events_uri;
	head_events_uri.method = HTTP_HEAD;
	head_events_uri.uri = "/events";
	head_events_uri.handler = http_head_events;
	ret = httpd_register_uri_handler(server, &head_events_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register HEAD /events handler (%i)", ret);

	// TODO: Register log events delete handler

	// Register get MQTT handler
	ESP_LOGV(TAG, "Registering get MQTT handler");
	httpd_uri_t get_mqtt_uri;
	get_mqtt_uri.method = HTTP_GET;
	get_mqtt_uri.uri = "/mqtt";
	get_mqtt_uri.handler = http_get_mqtt_handler;
	ret = httpd_register_uri_handler(server, &get_mqtt_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register get MQTT handler (%i)", ret);

	// Register set MQTT handler
	ESP_LOGV(TAG, "Registering set MQTT handler");
	httpd_uri_t set_mqtt_uri;
	set_mqtt_uri.method = HTTP_PUT;
	set_mqtt_uri.uri = "/mqtt";
	set_mqtt_uri.handler = http_set_mqtt_handler;
	ret = httpd_register_uri_handler(server, &set_mqtt_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register set MQTT handler (%i)", ret);

	// TODO: register timezone get handler
	ESP_LOGV(TAG, "Registering GET /tz handler");
	httpd_uri_t get_tz_uri;
	get_tz_uri.method = HTTP_GET;
	get_tz_uri.uri = "/tz";
	get_tz_uri.handler = http_get_tz;
	ret = httpd_register_uri_handler(server, &get_tz_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register GETT /tz handler (%i)", ret);

	// Register set timezone handler
	ESP_LOGV(TAG, "Registering PUT /tz handler");
	httpd_uri_t put_tz_uri;
	put_tz_uri.method = HTTP_PUT;
	put_tz_uri.uri = "/tz";
	put_tz_uri.handler = http_put_tz;
	ret = httpd_register_uri_handler(server, &put_tz_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register PUT /tz handler (%i)", ret);

    // Register restart handler
    ESP_LOGV(TAG, "Registering restart handler");
	httpd_uri_t restart_uri;
	restart_uri.method = HTTP_POST;
	restart_uri.uri = "/restart";
	restart_uri.handler = http_restart_handler;
	ret = httpd_register_uri_handler(server, &restart_uri);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to register restart handler (%i)", ret);

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

