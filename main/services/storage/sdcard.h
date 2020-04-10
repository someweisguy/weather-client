/*
 * sd.h
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#ifndef COMPONENTS_SDCARD_SDCARD_H_
#define COMPONENTS_SDCARD_SDCARD_H_


#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/sdmmc_host.h"
#include "cJSON.h"

#include "bsp.h"
#include "logger.h"

// Hardware pin defines
#define PIN_NUM_MISO (gpio_num_t) 19 // Adafruit Feather 32 Default
#define PIN_NUM_MOSI (gpio_num_t) 18 // Adafruit Feather 32 Default
#define PIN_NUM_CLK  (gpio_num_t) 5  // Adafruit Feather 32 Default
#define PIN_NUM_CS   (gpio_num_t) 21 // User Defined

esp_err_t sdcard_mount(const char* mount_point);
esp_err_t sdcard_unmount();

esp_err_t sdcard_file_exists(const char* file_name, bool &file_exists);
void sdcard_delete_file(const char* file_name);

esp_err_t sdcard_get_config_vals(const char *file_name, config_t &config);

esp_err_t store_json_string(const char *file_name, const char *json_string);

FILE *sdcard_open_file_readonly(const char* file_name);

#endif /* COMPONENTS_SDCARD_SDCARD_H_ */
