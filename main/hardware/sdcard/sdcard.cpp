/*
 * wifi.cpp
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#include "sdcard.h"
static const char *TAG { "sdcard" };

esp_err_t sdcard_mount() {
	// Initialize the SPI host
	ESP_LOGV(TAG, "Initializing SPI host");
	sdmmc_host_t host = SDSPI_HOST_DEFAULT();

	// Configure SPI SD device slot
	ESP_LOGV(TAG, "Configuring SPI device slot");
	sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
	slot_config.gpio_miso = static_cast<gpio_num_t>(PIN_NUM_MISO);
	slot_config.gpio_mosi = static_cast<gpio_num_t>(PIN_NUM_MOSI);
	slot_config.gpio_sck = static_cast<gpio_num_t>(PIN_NUM_SCLK);
	slot_config.gpio_cs = static_cast<gpio_num_t>(PIN_NUM_CS);

	// Configure filesystem mount options
	ESP_LOGV(TAG, "Configuring filesystem mount options");
	esp_vfs_fat_sdmmc_mount_config_t mount_config;
	mount_config.format_if_mount_failed = false;
	mount_config.allocation_unit_size = 16 * 1024;
	mount_config.max_files = 5;

	// Initialize SD card
	ESP_LOGV(TAG, "Initializing SD card");
	sdmmc_card_t *card;

	// Declare the mount point as '/' + TAG
	ESP_LOGV(TAG, "Setting the mount point");
	const size_t len { strlen(TAG) };
	char mount_point[len + 2] { "/" };
	strcat(mount_point, TAG);

	// Mount the filesystem - reduce the host frequency the card mounts
	ESP_LOGD(TAG, "Mounting the filesystem to '%s'", mount_point);
	size_t retries { 10 };
	esp_err_t mount_ret;
	do {
		mount_ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config,
				&mount_config, &card);
	} while (mount_ret != ESP_OK && --host.max_freq_khz > 0 && retries-- > 0);

	// Log error or warnings and return result
	if (mount_ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to mount the SD card (%x)", mount_ret);
	else if (host.max_freq_khz < 20000)
		ESP_LOGW(TAG, "The SD card host maximum frequency was set to %ukHz",
				host.max_freq_khz);
	return mount_ret;
}

esp_err_t sdcard_unmount() {
	ESP_LOGD(TAG, "Unmounting card");
	esp_err_t unmount_ret { esp_vfs_fat_sdmmc_unmount() };
	if (unmount_ret != ESP_OK)
		ESP_LOGE(TAG, "Unable to unmount the SD card (%x)", unmount_ret);
	return unmount_ret;
}
