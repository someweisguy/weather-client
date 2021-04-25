#pragma once

#include "esp_system.h"
#include <float.h>
#include "cJSON.h"
#include <math.h>
#include "serial.h"
#include <string.h>

#define BME_KEY         "bme280"
#define TEMPERATURE_KEY "temperature"
#define PRESSURE_KEY    "pressure"
#define HUMIDITY_KEY    "humidity"
#define DEW_POINT_KEY   "dew_point"

class bme280_t : public Sensor {
private:
  // bme280 register addresses
  const static uint8_t RESET_REGISTER       = 0xe0;
  const static uint8_t T1_TRIM_REGISTER     = 0x88;
  const static uint8_t H1_TRIM_REGISTER     = 0xa1;
  const static uint8_t H2_TRIM_REGISTER     = 0xe1;
  const static uint8_t H4_TRIM_REGISTER     = 0xe4;
  const static uint8_t H5_TRIM_REGISTER     = 0xe5;
  const static uint8_t H6_TRIM_REGISTER     = 0xe7;
  const static uint8_t DATA_START_REGISTER  = 0xf7;
  const static uint8_t CONFIG_REGISTER      = 0xf5;
  const static uint8_t CTRL_HUM_REGISTER    = 0xf2;
  const static uint8_t CTRL_MEAS_REGISTER   = 0xf4;
  const static uint8_t STATUS_REGISTER      = 0xf3;

  const uint8_t i2c_address;
  const double elevation_m;
  const discovery_t discovery[4] = {
      {
        .topic = "sensor/temperature",
        .config = {
          .device_class = "temperature",
          .expire_after = 310,
          .force_update = true,
          .icon = "mdi:thermometer",
          .name = "Temperature",
          .unit_of_measurement = "°F",
          .value_template = "{{ value_json." BME_KEY "." TEMPERATURE_KEY " | round(0) }}"
        },
      },
      {
        .topic = "sensor/pressure",
        .config = {
          .device_class = "pressure",
          .expire_after = 310,
          .force_update = true,
          .icon = "mdi:gauge",
          .name = "Pressure",
          .unit_of_measurement = "inHg",
          .value_template = "{{ value_json." BME_KEY "." PRESSURE_KEY " | round(1) }}"
        },
      },
      {
        .topic = "sensor/humidity",
        .config = {
          .device_class = "humidity",
          .expire_after = 310,
          .force_update = true,
          .icon = "mdi:water-percent",
          .name = "Humidity",
          .unit_of_measurement = "%",
          .value_template = "{{ value_json." BME_KEY "." HUMIDITY_KEY " | round(0) }}"
        }, 
      },
      {
        .topic = "sensor/dew_point",
        .config = {
          .device_class = nullptr,
          .expire_after = 310,
          .force_update = true,
          .icon = "mdi:weather-fog",
          .name = "Dew Point",
          .unit_of_measurement = "°F",
          .value_template = "{{ value_json." BME_KEY "." DEW_POINT_KEY " | round(0) }}"
        },
      }
    };
  struct {
    uint16_t t1;
    int16_t t2;
    int16_t t3;

    uint16_t p1;
    int16_t p2;
    int16_t p3;
    int16_t p4 ;
    int16_t p5;
    int16_t p6;
    int16_t p7;
    int16_t p8;
    int16_t p9;


    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4; // big endian, 12 bits long
    int16_t h5; // shares nibble with h4, 12 bits long
    int8_t h6;
  } dig;

  int32_t get_t_fine(const int32_t adc_T) const {
    // Function copied straight from the datasheet. 
    const int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig.t1 << 1))) * ((int32_t)dig.t2)) >> 11;
    const int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig.t1)) *  ((adc_T >> 4) - 
      ((int32_t)dig.t1))) >> 12) * ((int32_t)dig.t3)) >> 14;
    const int32_t T = var1 + var2;
    return T;
  }

  int32_t compensate_temperature(const int32_t t_fine) const {
    // Return temperature in 1/100ths of a degree Celsius
    return (t_fine * 5 + 128) >> 8;
  }

  uint32_t compensate_pressure(const int32_t t_fine, const int32_t adc_P) const {
    // Function copied straight from the datasheet. Returns pressure in Pascals * 256
    int64_t var1, var2, P;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig.p6;
    var2 = var2 + ((var1 * (int64_t)dig.p5) << 17);
    var2 = var2 + (((int64_t)dig.p4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig.p3) >> 8) + ((var1 * (int64_t)dig.p2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig.p1) >> 33;
    if (var1 == 0) return 0; // avoid divide by zero
    P = 1048576 - adc_P;
    P = (((P << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig.p9) * (P >> 13) * (P >> 13)) >> 25;
    var2 = (((int64_t)dig.p8) * P) >> 19;
    P = ((P + var1 + var2) >> 8) + (((int64_t)dig.p7) << 4);

    return P;
  }

  uint32_t compensate_humidity(const int32_t t_fine, const int32_t adc_H) const {
    // Function copied straight from the datasheet. Returns relative humidity * 1024.
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig.h4) << 20) - (((int32_t)dig.h5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
      (((((((v_x1_u32r * ((int32_t)dig.h6)) >> 10) * (((v_x1_u32r * ((int32_t)dig.h3)) >> 11) + ((int32_t)32768))) >> 10) + 
      ((int32_t)2097152)) * ((int32_t)dig.h2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig.h1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    const uint32_t H = (uint32_t)(v_x1_u32r >> 12);
    return H;
  }

  esp_err_t wait_bme_ready() const {
    // wait until the chip is done processing data
    uint8_t status;
    do {
      esp_err_t err = serial_i2c_read(i2c_address, STATUS_REGISTER, &status, 1,
        100 / portTICK_PERIOD_MS);
      if (err) return err;
      status &= 0x9; // only read bit 0 and 3
    } while (status);
    
    return ESP_OK;
  }

public:
  bme280_t(const uint8_t i2c_address, const double elevation_m) : Sensor("bme280"),
      i2c_address(i2c_address), elevation_m(elevation_m) {
  }

  int get_discovery(const discovery_t *&discovery) const {
    discovery = this->discovery;
    return sizeof(this->discovery) / sizeof(discovery_t);
  }

  esp_err_t setup() {
    // soft reset the chip
    const uint8_t reset_cmd = 0xb6;
    esp_err_t err = serial_i2c_write(i2c_address, RESET_REGISTER, &reset_cmd, 
      1, true, 100 / portTICK_PERIOD_MS);
    if (err) return err;

    err = wait_bme_ready();
    if (err) return err;

    // write config register
    // sets normal mode measurements to 1000ms, iir off, i2c mode on
    const uint8_t config_cmd = 0x80;
    err = serial_i2c_write(i2c_address, CONFIG_REGISTER, &config_cmd, 1, true, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;

    // write ctrl_hum register
    // sets humidity oversampling to x1
    const uint8_t ctrl_hum_cmd = 0x1;
    err = serial_i2c_write(i2c_address, CTRL_HUM_REGISTER, &ctrl_hum_cmd, 1, 
      true, 100 / portTICK_PERIOD_MS);
    if (err) return err;

    // write ctrl_meas register (must be done after writing ctrl_hum)
    // sets pressure and temperature oversampling to x1 and enables sleep mode
    const uint8_t ctrl_meas_cmd = 0x24;
    err = serial_i2c_write(i2c_address, CONFIG_REGISTER, &ctrl_meas_cmd, 1,
      true, 100 / portTICK_PERIOD_MS);
    if (err) return err;
    
    return ESP_OK;
  }

  esp_err_t ready() {
    // read the calibration data
    esp_err_t err = serial_i2c_read(i2c_address, T1_TRIM_REGISTER, &(dig.t1),
      24, 100 / portTICK_PERIOD_MS);
    if (err) return err;
    err = serial_i2c_read(i2c_address, H1_TRIM_REGISTER, &(dig.h1), 1, 
      100 / portTICK_PERIOD_MS);
    err = serial_i2c_read(i2c_address, H2_TRIM_REGISTER, &(dig.h2), 3, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;
    err = serial_i2c_read(i2c_address, H4_TRIM_REGISTER, &(dig.h4), 2, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;
    err = serial_i2c_read(i2c_address, H5_TRIM_REGISTER, &(dig.h5), 2, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;
    err = serial_i2c_read(i2c_address, H6_TRIM_REGISTER, &(dig.h6), 1, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;
    
    // h4 is little endian, h5 shares 4 bits with h4, and both are 12 bits long
    dig.h4 = (((dig.h4 & 0xfff) >> 8) | (dig.h4 << 4)) & 0xfff;
    dig.h5 = (dig.h5 >> 4) & 0xfff;

    return ESP_OK;
  }
  
  esp_err_t get_data(cJSON *json) {
    // write ctrl_meas register
    // sets forced mode, which forces a data measurement
    const uint8_t ctrl_meas_cmd = 0x25;
    esp_err_t err = serial_i2c_write(i2c_address, CTRL_MEAS_REGISTER, 
      &ctrl_meas_cmd, 1, true, 100 / portTICK_PERIOD_MS);
    if (err) return err;

    err = wait_bme_ready();
    if (err) return err;

    // get uncompensated data from the device
    uint8_t raw_data[8];
    err = serial_i2c_read(i2c_address, DATA_START_REGISTER, raw_data, 8, 
      100 / portTICK_PERIOD_MS);
    if (err) return err;

    // swap endianness and align
    int32_t adc_P = (raw_data[0] << 16 | raw_data[1] << 8 | raw_data[2]) >> 4;
    int32_t adc_T = (raw_data[3] << 16 | raw_data[4] << 8 | raw_data[5]) >> 4;
    int32_t adc_H = raw_data[6] << 8 | raw_data[7];

    const int32_t t_fine = get_t_fine(adc_T); // needed to compensate data
    double celsius, humidity; // needed for dew point and pressure

    cJSON *bme = cJSON_CreateObject();

    // get temperature value
    if (adc_T != 0x80000) {
      double temperature = compensate_temperature(t_fine) / 100.0; // C
      celsius = temperature;

      // convert to F
      temperature = (temperature * 9.0 / 5.0) + 32;

      cJSON_AddNumberToObject(bme, TEMPERATURE_KEY, temperature);
    } else {
      // temperature sampling must be turned on
      cJSON_Delete(bme);
      return ESP_ERR_INVALID_STATE;
    }

    // get pressure value
    if (adc_P != 0x80000) {
      double pressure = compensate_pressure(t_fine, adc_P) / 256; // Pa

      // convert pressure at sea level to pressure at current elevation
      const double M = 0.02897,          // molar mass of Eath's air (kg/mol)
                   g = 9.807665,         // gravitational constant (m/s^2)
                   R = 8.314462,         // universal gas constant (J/mol*K)
                   K = celsius + 273.15; // temperature in Kelvin
      pressure = pressure * exp((M * g) / (R * K) * elevation_m);
      
      // convert to inHg
      pressure /= 3386.3886666667;

      cJSON_AddNumberToObject(bme, PRESSURE_KEY, pressure);
    }

    // get humidity value
    if (adc_H != 0x800) {
      humidity = compensate_humidity(t_fine, adc_H) / 1024.0;

      cJSON_AddNumberToObject(bme, HUMIDITY_KEY, humidity);
    }

    // calculate the dew point
    if (adc_T != 0x80000 && adc_H != 0x800) {
      const double gamma = log(fmax(humidity, DBL_MIN) / 100.0) 
        + ((17.62 * celsius) / (243.12 + celsius));
      double dew_point = (243.12 * gamma) / (17.32 - gamma); // C
      
      // convert to F
      dew_point = (dew_point * 9.0 / 5.0) + 32;
      
      cJSON_AddNumberToObject(bme, DEW_POINT_KEY, dew_point);
    } 

    // add the json sub object to the main object
    cJSON_AddItemToObject(json, BME_KEY, bme);

    return ESP_OK;
  }

  esp_err_t sleep() {
    // don't need to do anything here
    // the bme280 automatically returns to sleep after forced mode measurement
    return ESP_OK;
  }  
};