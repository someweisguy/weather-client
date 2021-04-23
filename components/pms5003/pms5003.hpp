#pragma once

#include "esp_system.h"
#include "cJSON.h"
#include "serial.h"

#define PM_2_5_KEY  "pm_2_5"
#define PM_10_KEY   "pm_10"

class pms5003_t : public Sensor
{
private:
public:
  pms5003_t() : Sensor("pms5003") {
    discovery = new discovery_t[2] {
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
          },
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
          },
        }
      };
  }

  virtual esp_err_t setup() {
    return ESP_OK;
  }

  virtual esp_err_t ready() {
    return ESP_OK;
  }

  virtual esp_err_t get_data(cJSON *json) {
    return ESP_OK;
  }

  virtual esp_err_t sleep() {
    return ESP_OK;
  }
}
