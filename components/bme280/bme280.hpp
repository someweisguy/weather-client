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
  bme280_t(const uint8_t i2c_address, const double elevation) : Sensor(4),
      i2c_address(i2c_address), elevation(elevation) {

    // temperature value
    int topic_num = 0;
    this->discover_topics[topic_num] = "test/temperature/config";
    this->discover_strings[topic_num] = cJSON_CreateObject();
    cJSON_AddStringToObject(this->discover_strings[topic_num], "device_class", "temperature");
    this->init_sensor_discovery(this->discover_strings[topic_num], 
      310, 
      true,
      "icon",
      "temperature",
      2,
      "state_topic",
      "unique_id",
      "F",
      "{{ json.temperature }}"
    );

    // humidity value
    topic_num++;
    this->discover_topics[topic_num] = "test/humidity/config";
    this->discover_strings[topic_num] = cJSON_CreateObject();
    cJSON_AddStringToObject(this->discover_strings[topic_num], "device_class", "humidity");
    this->init_sensor_discovery(this->discover_strings[topic_num], 
      310, 
      true,
      "icon",
      "humidity",
      2,
      "state_topic",
      "unique_id",
      "%",
      "{{ json.humidity }}"
    );

    // pressure value
    topic_num++;
    this->discover_topics[topic_num] = "test/pressure/config";
    this->discover_strings[topic_num] = cJSON_CreateObject();
    cJSON_AddStringToObject(this->discover_strings[topic_num], "device_class", "pressure");
    this->init_sensor_discovery(this->discover_strings[topic_num], 
      310, 
      true,
      "icon",
      "pressure",
      2,
      "state_topic",
      "unique_id",
      "inHg",
      "{{ json.pressure }}"
    );

    // dew point value
    topic_num++;
    this->discover_topics[topic_num] = "test/dew_point/config";
    this->discover_strings[topic_num] = cJSON_CreateObject();
    this->init_sensor_discovery(this->discover_strings[topic_num], 
      310, 
      true,
      "icon",
      "dew point",
      2,
      "state_topic",
      "unique_id",
      "F",
      "{{ json.dew_point }}"
    );



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