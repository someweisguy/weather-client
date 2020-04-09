/*
 * wifi.cpp
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#include "sdcard.h"
static const char* TAG { "sdcard" };
static char* fs_root { 0 };

esp_err_t sdcard_mount(const char *mount_point) {
	verbose(TAG, "Configuring SD card");
	sdmmc_host_t host = SDSPI_HOST_DEFAULT();

	// Don't use off-brand microSD cards - https://github.com/espressif/esp-idf/issues/965
	--host.max_freq_khz;

	sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
	slot_config.gpio_miso = PIN_NUM_MISO;
	slot_config.gpio_mosi = PIN_NUM_MOSI;
	slot_config.gpio_sck  = PIN_NUM_CLK;
	slot_config.gpio_cs   = PIN_NUM_CS;

	esp_vfs_fat_sdmmc_mount_config_t mount_config {};
	mount_config.format_if_mount_failed = false;
	mount_config.allocation_unit_size = 512; // always this for SD cards
	mount_config.max_files = 5;

	// TODO: esp_vfs_fat_sdmmc_mount() is a convenience function; replace it
	sdmmc_card_t* card;
	verbose(TAG, "Mounting SD card to \"%s\"", mount_point);
	const esp_err_t mount_ret { esp_vfs_fat_sdmmc_mount(mount_point, &host,
			&slot_config, &mount_config, &card) };

	// Fail quick
	if (mount_ret != ESP_OK)
		return mount_ret;

	const size_t mount_point_len { strlen(mount_point) };
	fs_root = new char[mount_point_len + 1];
	strncpy(fs_root, mount_point, mount_point_len);
	return ESP_OK;
}

esp_err_t sdcard_unmount() {
	// TODO esp_vfs_fat_sdmmc_unmount is a convenience function; replace it
	verbose(TAG, "Unmounting SD card");
	return esp_vfs_fat_sdmmc_unmount();
}

esp_err_t sdcard_file_exists(const char* file_name, bool &file_exists) {
	// Construct a string with the full path of the file
	const size_t path_len { strlen(fs_root) + strlen(file_name) };
	char full_file_path[path_len + 1];
	strcpy(full_file_path, fs_root);
	strcat(full_file_path, file_name);

	struct stat buffer;
	file_exists = stat(full_file_path, &buffer) == 0;
	return ESP_OK;
}

void sdcard_delete_file(const char* file_name) {
	// Construct a string with the full path of the file
	const size_t path_len { strlen(fs_root) + strlen(file_name) };
	char full_file_path[path_len + 1];
	strcpy(full_file_path, fs_root);
	strcat(full_file_path, file_name);
	remove(full_file_path);
}

esp_err_t get_wifi_credentials(const char* file_name, char *ssid, char *pass) {
	// Construct a string with the full path of the file
	const size_t path_len { strlen(fs_root) + strlen(file_name) };
	char full_file_path[path_len + 1];
	strcpy(full_file_path, fs_root);
	strcat(full_file_path, file_name);

	// Open the file
	verbose(TAG, "Opening \"%s\"", full_file_path);
	FILE *f { fopen(full_file_path, "r") };
	if (f != nullptr) {
		// Read the file into memory then close the file
		verbose(TAG, "Reading file into memory");
		fseek(f, 0, SEEK_END);
		const long fsize { ftell(f) };
		// Don't read more than 1KB into memory
		if (fsize > 1024)
			return ESP_FAIL;
		rewind(f);
		char json_string[fsize + 1];
		fread(json_string, 1, fsize, f);
		fclose(f);

		// Parse the JSON object
		cJSON *root = cJSON_Parse(json_string);
		verbose(TAG, "Parsing JSON");
		// FIXME: Program crashes if field names do not exist in file
		strncpy(ssid, cJSON_GetObjectItem(root, "ssid")->valuestring, 32);
		strncpy(pass, cJSON_GetObjectItem(root, "pass")->valuestring, 64);
		cJSON_Delete(root);
		return ESP_OK;

	} else { // f == nullptr
		return ESP_FAIL;
	}
}

esp_err_t get_mqtt_credentials(const char* file_name, char **mqtt_broker,
		char **topic) {
	// Construct a string with the full path of the file
	const size_t path_len { strlen(fs_root) + strlen(file_name) };
	char full_file_path[path_len + 1];
	strcpy(full_file_path, fs_root);
	strcat(full_file_path, file_name);

	// Open the file
	verbose(TAG, "Opening \"%s\"", full_file_path);
	FILE *f { fopen(full_file_path, "r") };
	if (f != nullptr) {
		// Read the file into memory then close the file
		verbose(TAG, "Reading file into memory");
		fseek(f, 0, SEEK_END);
		const long fsize { ftell(f) };
		// Don't read more than 1KB into memory
		if (fsize > 1024)
			return ESP_FAIL;
		rewind(f);
		char json_string[fsize + 1];
		fread(json_string, 1, fsize, f);
		fclose(f);

		// Parse the JSON object
		cJSON *root = cJSON_Parse(json_string);
		verbose(TAG, "Parsing JSON");
		// FIXME: Program crashes if field names do not exist in file
		*mqtt_broker = strndup(cJSON_GetObjectItem(root, "mqtt")->valuestring, 255);
		*topic = strndup(cJSON_GetObjectItem(root, "topic")->valuestring, 255);
		cJSON_Delete(root);
		return ESP_OK;

	} else { // f == nullptr
		return ESP_FAIL;
	}
}

esp_err_t store_json_string(const char *file_name, const char *json_string) {
	// Construct a string with the full path of the file
	const size_t path_len { strlen(fs_root) + strlen(file_name) };
	char full_file_path[path_len + 1];
	strcpy(full_file_path, fs_root);
	strcat(full_file_path, file_name);

	// Open the file
	verbose(TAG, "Opening \"%s\"", full_file_path);
	FILE *f { fopen(full_file_path, "a+") };
	if (f != nullptr) {
		// Store the string then close the file
		verbose(TAG, "Storing the JSON string to file");
		fputs(json_string, f);
		fputc('\n', f);
		fclose(f);
		return ESP_OK;

	} else { // f == nullptr
		return ESP_FAIL;
	}
}

FILE *sdcard_open_file_readonly(const char* file_name) {
	// Construct a string with the full path of the file
	const size_t path_len { strlen(fs_root) + strlen(file_name) };
	char full_file_path[path_len + 1];
	strcpy(full_file_path, fs_root);
	strcat(full_file_path, file_name);

	return fopen(full_file_path, "r");
}
