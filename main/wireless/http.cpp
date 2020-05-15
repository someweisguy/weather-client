/*
 * http.cpp
 *
 *  Created on: May 10, 2020
 *      Author: Mitch
 */

#include "http.h"
static const char *TAG { "http" };
static httpd_handle_t http_server { nullptr };

static esp_err_t http_get_handler(httpd_req_t *req) {
	ESP_LOGD(TAG, "Handling HTTP_GET event");




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


    return ESP_OK;
}

esp_err_t http_start() {
	if (http_server != nullptr)
		return ESP_OK;

	// Configure and start the web server
	ESP_LOGD(TAG, "Starting web server");
    const httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    const esp_err_t start_ret { httpd_start(&http_server, &config) };

    // Set http handler
    if (start_ret == ESP_OK) {
    	httpd_uri_t get;
    	get.uri = "/";
    	get.method = HTTP_GET;
    	get.handler = http_get_handler;
    	get.user_ctx = nullptr;
        httpd_register_uri_handler(http_server, &get);
    }

    return start_ret;
}

esp_err_t http_stop() {
	if (http_server == nullptr)
		return ESP_OK;

	httpd_stop(http_server);
	http_server = nullptr;
	return ESP_OK;
}

