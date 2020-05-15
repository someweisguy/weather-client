/*
 * power.cpp
 *
 *  Created on: May 14, 2020
 *      Author: Mitch
 */

#include "power.h"
static const char *TAG { "power" };

void vTaskCode(void *pvParameters) {

	// Read the value of the interrupt pin to determine what power we are using
	const bool has_usb_power { gpio_get_level(static_cast<gpio_num_t>(
			BATT_INTERRUPT_PIN)) };
	gpio_set_level(static_cast<gpio_num_t>(FEATHER_LED_PIN), !has_usb_power);

	if (!has_usb_power) {
		ESP_LOGW(TAG, "Using battery power");
	} else {
		ESP_LOGI(TAG, "Using USB power");
	}

	vTaskDelete(nullptr);
}

static void IRAM_ATTR gpio_isr_handler(void *args) {
	xTaskCreate(vTaskCode, "task", 2048, nullptr, tskIDLE_PRIORITY, nullptr);
}

void configure_battery_interrupt() {
	ESP_LOGD(TAG, "Configuring low power interrupt");

	// Setup LED
	gpio_pad_select_gpio(FEATHER_LED_PIN);
	gpio_set_direction(static_cast<gpio_num_t>(FEATHER_LED_PIN), GPIO_MODE_OUTPUT);

	// Setup interrupt pin
	gpio_set_direction(static_cast<gpio_num_t>(BATT_INTERRUPT_PIN), GPIO_MODE_INPUT);
	gpio_set_intr_type(static_cast<gpio_num_t>(BATT_INTERRUPT_PIN), GPIO_INTR_ANYEDGE);

	// Setup and enable the lowest priority interrupt
	gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
	gpio_isr_handler_add(static_cast<gpio_num_t>(BATT_INTERRUPT_PIN),
			gpio_isr_handler, nullptr);
	gpio_intr_enable(static_cast<gpio_num_t>(BATT_INTERRUPT_PIN));

	// Set the initial state
	//gpio_isr_handler(nullptr);
}
