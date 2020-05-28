/*
 * http.h
 *
 *  Created on: May 10, 2020
 *      Author: Mitch
 */

#ifndef MAIN_WIRELESS_INCLUDE_HTTP_H_
#define MAIN_WIRELESS_INCLUDE_HTTP_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "helpers.h"

#define MAX_CHUNK_SIZE 2048
#define USER_AGENT	   CONFIG_LWIP_LOCAL_HOSTNAME "/0.1"
#define HTTPD_201      "201 Created"
#define HTTPD_202	   "202 Accepted"

bool http_start();
bool http_stop();

#endif /* MAIN_WIRELESS_INCLUDE_HTTP_H_ */
