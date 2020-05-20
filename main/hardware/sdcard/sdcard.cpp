/*
 * wifi.cpp
 *
 *  Created on: Mar 28, 2020
 *      Author: Mitch
 */

#include "sdcard.h"
static const char *TAG { "sdcard" };
static volatile bool is_mounted { false };
SemaphoreHandle_t detect_semaphore;

static void sdcard_monitor_task(void *args) {
	const gpio_num_t CD_PIN { static_cast<gpio_num_t>(PIN_NUM_CD) };
	while (true) {
		if(xSemaphoreTake(detect_semaphore, portMAX_DELAY) == pdTRUE) {
			// Debounce
			bool cd_level { gpio_get_level(CD_PIN) };
			for (int c = 0; c < 8; ++c) {
				if (cd_level != gpio_get_level(CD_PIN)) {
					cd_level = !cd_level;
					c = 0;
				}
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}

			// Mount or unmount the card depending on card detect pin
			if (!cd_level) sdcard_mount();
			else sdcard_unmount();
		}
	}
}

static void IRAM_ATTR sdcard_isr(void *args) {
	xSemaphoreGiveFromISR(detect_semaphore, nullptr);
}

bool sdcard_mount() {
	if (is_mounted) {
		ESP_LOGD(TAG, "The filesystem is already mounted");
		return true;
	}

	// Initialize the SPI host
	ESP_LOGV(TAG, "Initializing the SPI host");
	sdmmc_host_t host = SDSPI_HOST_DEFAULT(); // @suppress("Invalid arguments")

	// Configure SPI SD device slot
	ESP_LOGV(TAG, "Configuring SPI device slot");
	sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
	slot_config.gpio_miso = static_cast<gpio_num_t>(PIN_NUM_MISO);
	slot_config.gpio_mosi = static_cast<gpio_num_t>(PIN_NUM_MOSI);
	slot_config.gpio_sck = static_cast<gpio_num_t>(PIN_NUM_SCLK);
	slot_config.gpio_cs = static_cast<gpio_num_t>(PIN_NUM_CS);
	// TODO: what happens when gpio_cd is defined?
	//slot_config.gpio_cd = static_cast<gpio_num_t>(PIN_NUM_CD);

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
	ESP_LOGI(TAG, "Mounting the filesystem to '%s'", mount_point);
	size_t retries { 10 };
	esp_err_t mount_ret;
	do {
		mount_ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config,
				&mount_config, &card);
	} while (mount_ret != ESP_OK && --host.max_freq_khz > 0 && retries-- > 0);

	// Log error or warnings and return result
	if (mount_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to mount the filesystem (%i)", mount_ret);
		return false;
	} else if (host.max_freq_khz < 20000)
		ESP_LOGW(TAG, "The SD card host max frequency was set to %ukHz",
				host.max_freq_khz);
	is_mounted = true;
	return true;
}

bool sdcard_unmount() {
	if (!is_mounted) {
		ESP_LOGD(TAG, "The filesystem is already unmounted");
		return true;
	}

	ESP_LOGI(TAG, "Unmounting the filesystem");
	esp_err_t unmount_ret { esp_vfs_fat_sdmmc_unmount() };
	if (unmount_ret != ESP_OK) {
		ESP_LOGE(TAG, "Unable to unmount the filesystem (%i)", unmount_ret);
		return false;
	}
	is_mounted = false;
	return true;
}

bool sdcard_is_mounted() {
	return is_mounted;
}

void sdcard_auto_detect() {
	// Setup card detect pin
	const gpio_num_t CD_PIN { static_cast<gpio_num_t>(PIN_NUM_CD) };
	gpio_set_direction(CD_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(CD_PIN, GPIO_PULLUP_ONLY);
	gpio_set_intr_type(CD_PIN, GPIO_INTR_ANYEDGE);
	gpio_pullup_en(CD_PIN);

	// Setup and enable the interrupt
	gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
	gpio_isr_handler_add(CD_PIN, sdcard_isr,
			nullptr);
	gpio_intr_enable(CD_PIN);

	// Set the starting state
	detect_semaphore = xSemaphoreCreateBinary();
	xTaskCreate(sdcard_monitor_task, "detect_sdcard", 2048, nullptr,
			tskIDLE_PRIORITY, nullptr);
	xSemaphoreGive(detect_semaphore);
}
