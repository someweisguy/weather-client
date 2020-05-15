/*
 * sd.h
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#ifndef MAIN_HARDWARE_SDCARD_SDCARD_H_
#define MAIN_HARDWARE_SDCARD_SDCARD_H_

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <cstdio>
#include <cstring>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/adc.h"

#define PIN_NUM_MISO 19 // Adafruit Feather 32 Default
#define PIN_NUM_MOSI 18 // Adafruit Feather 32 Default
#define PIN_NUM_SCLK  5 // Adafruit Feather 32 Default
#define PIN_NUM_CS   21 // User Defined


esp_err_t sdcard_mount();
esp_err_t sdcard_unmount();

#endif /* MAIN_HARDWARE_SDCARD_SDCARD_H_ */