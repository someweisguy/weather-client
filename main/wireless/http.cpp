/*
 * http.cpp
 *
 *  Created on: May 10, 2020
 *      Author: Mitch
 */

#include "http.h"
static const char *TAG { "http" };
static httpd_handle_t server { nullptr };

// TODO: Write code to read the log file over http

// TODO: clean up http source
// TODO: figure out how to download log file over http

static esp_err_t http_restart_handler(httpd_req_t *r) {
	ESP_LOGV(TAG, "Handling HTTP_RESTART_REQUST event");

	ESP_LOGD(TAG, "Responding to restart request");
	esp_err_t resp_ret { httpd_resp_send(r, nullptr, 0) };
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

static esp_err_t http_log_download(httpd_req_t *req) {
	ESP_LOGD(TAG, "Handling HTTP_GET event");
	/*
	size_t mem = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
	ESP_LOGW(TAG, "Largest free block of memory is %u", mem);
	int chunk_cap = mem;
	char *chunk = new char[chunk_cap];
	FILE *fd = fopen("/sdcard/events.log", "r");
	fseek(fd, 0, SEEK_END);
	size_t size = ftell(fd);
	rewind(fd);
	const char* file_name = "events.log";
	const char *fn_fmt = "attachment; filename=%s;";
	int fmt_size = snprintf(nullptr, 0, fn_fmt, file_name);
	char fn_value[fmt_size + 1];
	sprintf(fn_value, fn_fmt, file_name);
	const char *fs_fmt = "%u";
	int sz_size = snprintf(nullptr, 0, fs_fmt, size);
	char fs_value[sz_size + 1];
	sprintf(fs_value, fs_fmt, size);
	httpd_resp_set_hdr(req, "Content-Disposition", fn_value);
	httpd_resp_set_hdr(req, "Content-Length", fs_value);
	httpd_resp_set_type(req, "application/octet-stream");

	//set_content_type_from_file(req, "/sdcard/events.log");

	size_t chunksize;
	do {
		chunksize = fread(chunk, 1, chunk_cap, fd);
		if (chunksize > 0) {
			if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
				fclose(fd);
				ESP_LOGE(TAG, "File sending failed!");
				httpd_resp_sendstr_chunk(req, NULL);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
			   return ESP_FAIL;
		   }
		}
	} while (chunksize != 0);
	fclose(fd);
	httpd_resp_send_chunk(req, NULL, 0);
	delete[] chunk;
	*/

	char restart[12];
	size_t len { httpd_req_get_url_query_len(req) };
	char query[len + 1];
	httpd_req_get_url_query_str(req, query, len);
	ESP_LOGD(TAG, "Got query: %s", query);
	esp_err_t ret = httpd_query_key_value(query, "restart", restart, 255);
	if (ret == ESP_OK) {
		auto restart_task = [] (void *args) { esp_restart(); };
		xTaskCreate(restart_task, "restart", 2048, nullptr, 11, nullptr);
		return ESP_OK;
	}



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

    // Set http restart handler
    ESP_LOGD(TAG, "Registering restart handler");
	httpd_uri_t restart;
	restart.method = HTTP_POST;
	restart.uri = "/restart";
	restart.handler = http_restart_handler;
	esp_err_t ret = httpd_register_uri_handler(server, &restart);
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

