#pragma once

#include "esp_system.h"
#include "cJSON.h"
#include "serial.h"

#define PM_2_5_KEY  "pm_2_5"
#define PM_10_KEY   "pm_10"

class pms5003_t : public Sensor {
private:
  const discovery_t discovery[2] {
        {
          .topic = "test/sensor/pm2.5/config",
          .config = {
            .device_class = nullptr,
            .expire_after = 310,
            .force_update = true,
            .icon = "mdi:smog",
            .name = "PM 2.5",
            .unit_of_measurement = "μg/m³",
            .value_template = "{{ json." PM_2_5_KEY " }}"
          }
        },
        {
          .topic = "test/sensor/pm10/config",
          .config = {
            .device_class = nullptr, 
            .expire_after = 310, 
            .force_update = true, 
            .icon = "mdi:smog", 
            .name = "PM 10", 
            .unit_of_measurement = "μg/m³", 
            .value_template = "{{ json." PM_10_KEY " }}"
          }
        }
      };

public:
  pms5003_t() : Sensor("pms5003") {
  }

  int get_discovery(const discovery_t *&discovery) const {
    discovery = this->discovery;
    return sizeof(this->discovery) / sizeof(discovery_t);
  }

  esp_err_t setup() {
    // put the sensor in passive mode
    const uint8_t passive_cmd[] = {0x42, 0x4d, 0xe1, 0x00, 0x00, 0x01, 0x70};
    esp_err_t err = serial_uart_write(passive_cmd, 7);
    if (err) return err;

    // put the sensor to sleep
    const uint8_t sleep_cmd[] = {0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73};
    err = serial_uart_write(sleep_cmd, 7);
    if (err) return err;

    return ESP_OK;
  }

  esp_err_t ready() {
    // wake up the sensor
    const uint8_t wake_cmd[] = {0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74};
    esp_err_t err = serial_uart_write(wake_cmd, 7);
    if (err) return err;

    return ESP_OK;
  }

  esp_err_t get_data(cJSON *json) {
    // TODO
    return ESP_OK;
  }

  esp_err_t sleep() {
    // put the sensor to sleep
    const uint8_t sleep_cmd[] = {0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73};
    esp_err_t err = serial_uart_write(sleep_cmd, 7);
    if (err) return err;

    return ESP_OK;
  }
};
