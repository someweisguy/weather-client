#pragma once

#include "driver/rtc_io.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "sensor.hpp"
#include "serial.h"
#include "wireless.h"

#define PM_2_5_KEY  "pm_2_5"
#define PM_10_KEY   "pm_10"

class pms5003_t : public sensor_t {
private:
  static const discovery_t discoveries[];
  

  gpio_num_t power_rtc_gpio;
  struct pms_data_t { 
    uint16_t start_word;
    uint16_t len;
    struct {
      uint16_t pm1;
      uint16_t pm2_5;
      uint16_t pm10;
    } standard;
    struct {
      uint16_t pm1;
      uint16_t pm2_5;
      uint16_t pm10;
    } atmospheric;
    uint16_t num_particles_0_3um;
    uint16_t num_particles_0_5um;
    uint16_t num_particles_1um;
    uint16_t num_particles_2_5um;
    uint16_t num_particles_5um;
    uint16_t num_particles_10um;
    uint16_t : 16; // reserved
    uint16_t checksum;
  };


public:
  pms5003_t(gpio_num_t power_rtc_gpio) : sensor_t("pms5003", discoveries, 2), 
      power_rtc_gpio(power_rtc_gpio) {
    // do nothing...
  }

  esp_err_t setup() override {
    // setup the rtc gpio for the power ic
    rtc_gpio_init(power_rtc_gpio);
    rtc_gpio_set_direction(power_rtc_gpio, RTC_GPIO_MODE_OUTPUT_ONLY);

    // turn off the power for the sensor
    esp_err_t err = rtc_gpio_set_level(power_rtc_gpio, 0);
    if (err) return err;

    return ESP_OK;
  }

  esp_err_t ready() override {
    // turn on the power for the sensor
    esp_err_t err = rtc_gpio_set_level(power_rtc_gpio, 1);
    if (err) return err;

    auto set_passive_mode = [](void *args) {
      // wait for sensor to wake up
      vTaskDelay(1500 / portTICK_PERIOD_MS);

      // set to passive mode
      const uint8_t passive_cmd[] = {0x42, 0x4d, 0xe1, 0x00, 0x00, 0x01, 0x70};
      serial_uart_write(passive_cmd, sizeof(passive_cmd), 
        100 / portTICK_PERIOD_MS);

      vTaskDelete(nullptr);
    };

    // create the task to set the pms5003 to passive mode
    xTaskCreate(set_passive_mode, "pms5003_set_passive_mode", 2048, nullptr, 0,
      nullptr);

    return ESP_OK;
  }

  esp_err_t get_data(cJSON *json) override {
    // flush the uart rx buffer
    serial_uart_flush();

    // force a measurement
    const uint8_t read_cmd[] = {0x42, 0x4d, 0xe2, 0x00, 0x00, 0x01, 0x71};
    esp_err_t err = serial_uart_write(read_cmd, sizeof(read_cmd),
      100 / portTICK_PERIOD_MS);
    if (err) return err;

    // read the data from the measurement
    pms_data_t data;
    err = serial_uart_read(&data, sizeof(data), 
      5000 / portTICK_PERIOD_MS);
    if (err) return err;

    // swap data endianness
    uint8_t *const raw_data = reinterpret_cast<uint8_t *>(&data);
    for (int i = 3; i < 32; i += 2) {
      const uint8_t temp = raw_data[i - 1];
      raw_data[i - 1] = raw_data[i];
      raw_data[i] = temp;
    }

    // verify checksum
    uint16_t checksum = 0;
    for (int i = 0; i < 30; ++i) checksum += raw_data[i];
    if (checksum != data.checksum) return ESP_ERR_INVALID_CRC;

    cJSON_AddNumberToObject(json, PM_2_5_KEY, data.atmospheric.pm2_5);
    cJSON_AddNumberToObject(json, PM_10_KEY, data.atmospheric.pm10);

    return ESP_OK;
  }

  esp_err_t sleep() override {
    // turn off the power for the sensor
    esp_err_t err = rtc_gpio_set_level(power_rtc_gpio, 0);
    if (err) return err;

    return ESP_OK;
  }
};
