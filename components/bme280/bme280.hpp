#pragma once

#include "esp_system.h"
#include "cJSON.h"
#include "serial.h"

#define I2C_ADDRESS 0x76
#define REG_CHIP_ID 0xd0
#define REG_RESET 0xe0
#define REG_CTRL_HUM 0xf2
#define REG_STATUS 0xf3
#define REG_CTRL_MEAS 0xf4
#define REG_CONFIG 0xf5
#define REG_DATA_START 0xf7
#define REG_TRIM_T1_TO_H1 0x88
#define REG_TRIM_H2_TO_H6 0xe1

class bme280_t : public Sensor
{
private:
  const uint8_t i2c_address;
  const double elevation;

  const uint8_t RESET_REGISTER = 0xe0;

  struct
  {
    uint16_t t1;
    int16_t t2;
    int16_t t3;

    uint16_t p1;
    int16_t p2;
    int16_t p3;
    int16_t p4;
    int16_t p5;
    int16_t p6;
    int16_t p7;
    int16_t p8;
    int16_t p9;

    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4;
    int16_t h5;
    int8_t h6;
  } dig;

public:
  bme280_t(const uint8_t i2c_address, const double elevation) : Sensor("bme280"),
      i2c_address(i2c_address), elevation(elevation) {
    this->discovery = new discovery_t[4] {
      {
        .topic = "test/temperature/config",
        .config = {
          .expire_after = 310,
          .force_update = true,
          .icon = "icon_temperature",
          .name = "temperature",
          .qos = 2,
          .state_topic = "state_topic",
          .unique_id = "unique_id",
          .unit_of_measurement = "F",
          .value_template = "{{ json.temperature }}"
        }
      },
      {
        .topic = "test/humidity/config",
        .config = {
          .expire_after = 310,
          .force_update = true,
          .icon = "icon_humidity",
          .name = "humidity",
          .qos = 2,
          .state_topic = "state_topic",
          .unique_id = "unique_id",
          .unit_of_measurement = "%",
          .value_template = "{{ json.humidity }}"
        }
      },
      {
        .topic = "test/pressure/config",
        .config = {
          .expire_after = 310,
          .force_update = true,
          .icon = "icon_pressure",
          .name = "pressure",
          .qos = 2,
          .state_topic = "state_topic",
          .unique_id = "unique_id",
          .unit_of_measurement = "inHg",
          .value_template = "{{ json.pressure }}"
        }
      },
      {
        .topic = "test/dew_point/config",
        .config = {
          .expire_after = 310,
          .force_update = true,
          .icon = "icon_dew_point",
          .name = "dew_point",
          .qos = 2,
          .state_topic = "state_topic",
          .unique_id = "unique_id",
          .unit_of_measurement = "F",
          .value_template = "{{ json.dew_point }}"
        }
      }
    };
  };

  esp_err_t setup() const {
    const uint8_t rst_cmd = 0xb6;
    esp_err_t err = serial_i2c_write(i2c_address, RESET_REGISTER, 
      reinterpret_cast<void *>(const_cast<uint8_t *>(&rst_cmd)), 1, true, 
      100 / portTICK_PERIOD_MS);
    return err;
  }

  /*

  virtual esp_err_t get_data(cJSON *json) const
  {
    return ESP_OK;
  }

  */
};