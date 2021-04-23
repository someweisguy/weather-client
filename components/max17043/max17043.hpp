#pragma once

#include "esp_system.h"
#include "cJSON.h"
#include "serial.h"

#define BATTERY_KEY   "battery"

class max17043_t : public Sensor {
private:
  // max17043 register addresses
  const static uint8_t COMMAND_REGISTER = 0xfe;
  const static uint8_t CONFIG_REGISTER = 0x0c;

  const uint8_t i2c_address;
  const discovery_t discovery[1] {
        {
          .topic = "test/sensor/battery/config",
          .config = {
            .device_class = "battery",
            .expire_after = 310,
            .force_update = true,
            .icon = nullptr,
            .name = "Battery",
            .unit_of_measurement = "%",
            .value_template = "{{ json." BATTERY_KEY " }}"
          }
        }
    };
  


public:
  max17043_t(uint8_t i2c_address) : Sensor("max17043"), 
      i2c_address(i2c_address) {
  }

  int get_discovery(const discovery_t *&discovery) const {
    discovery = this->discovery;
    return sizeof(this->discovery) / sizeof(discovery_t);
  }

  esp_err_t setup() {
    // send the reset command
    const uint8_t reset_cmd[] = {0x54, 0x00};
    esp_err_t err = serial_i2c_write(i2c_address, COMMAND_REGISTER, reset_cmd,
      2, false, 100 / portTICK_PERIOD_MS);
    if (err) return err;

    // put the max17043 to sleep
    const uint8_t sleep_cmd[] = {0x97, 0x80};
    err = serial_i2c_write(i2c_address, CONFIG_REGISTER, sleep_cmd, 2, true, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;

    return ESP_OK;
  }

  esp_err_t ready() {
    // wake up the max17043
    const uint8_t wake_cmd[] = {0x97, 0x00};
    esp_err_t err = serial_i2c_write(i2c_address, CONFIG_REGISTER, wake_cmd, 2,
      true, 100 / portTICK_PERIOD_MS);
    if (err) return err;
    
    return ESP_OK;
  }

  esp_err_t get_data(cJSON *json) {
 
    return ESP_OK;
  }

  esp_err_t sleep() {
    // put the max17043 to sleep
    const uint8_t sleep_cmd[] = {0x97, 0x80};
    err = serial_i2c_write(i2c_address, CONFIG_REGISTER, sleep_cmd, 2, true, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;

    return ESP_OK;
  }
};
