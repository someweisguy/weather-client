/*
 * http.h
 *
 *  Created on: May 10, 2020
 *      Author: Mitch
 */

#ifndef MAIN_WIRELESS_INCLUDE_HTTP_H_
#define MAIN_WIRELESS_INCLUDE_HTTP_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"


esp_err_t http_start();
esp_err_t http_stop();

#endif /* MAIN_WIRELESS_INCLUDE_HTTP_H_ */
