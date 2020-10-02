#pragma once

#include "esp_system.h"

#define BME280_OVERSAMPLING_OFF 0
#define BME280_OVERSAMPLING_x1 1
#define BME280_OVERSAMPLING_x2 2
#define BME280_OVERSAMPLING_x4 3
#define BME280_OVERSAMPLING_x8 4
#define BME280_OVERSAMPLING_x16 5

#define BME280_SLEEP_MODE 0
#define BME280_FORCED_MODE 1
#define BME280_NORMAL_MODE 3

#define BME280_STANDBY_0_5_MS 0
#define BME280_STANDBY_62_5_MS 1

#define BME280_STANDBY_125_MS 2
#define BME280_STANDBY_250_MS 3
#define BME280_STANDBY_500_MS 4
#define BME280_STANDBY_1000_MS 5
#define BME280_STANDBY_10_MS 6
#define BME280_STANDBY_20_MS 7

#define BME280_FILTER_OFF 0
#define BME280_FILTER_2 1
#define BME280_FILTER_4 2
#define BME280_FILTER_8 3
#define BME280_FILTER_16 4

#define BME280_WEATHER_MONITORING { .config = {.spi3w_en = 0, .t_sb = 0, .filter = 0}, .ctrl_meas = {.mode = 1, .osrs_p = 1, .osrs_t = 1}, .ctrl_hum = {.osrs_h = 1}};

typedef struct
{
    int64_t pressure;
    float temperature;
    float humidity;
    float dew_point;
} bme280_data_t;

typedef struct
{
    union
    {
        struct
        {
            uint8_t spi3w_en : 1; // Enables 3-wire SPI interface when set to '1'.
            uint8_t : 1;
            uint8_t filter : 3; // Controls the time constant of the IIR filter.
            uint8_t t_sb : 3;   // Controls inactive duration in normal mode.
        };
        uint8_t val;
    } config; // The "config" register sets the rate, filter and interface options of the device. WRites to the "config register" in normal mode may be ignored. In sleep mode writes are not ignored.
    union
    {
        struct
        {
            uint8_t mode : 2;   // Controls the sensor mode of the device.
            uint8_t osrs_p : 3; // Controls oversampling of pressure data.
            uint8_t osrs_t : 3; // Controls oversampling of temperature data.
        };
        uint8_t val;
    } ctrl_meas; // The "ctrl_meas" register sets the pressure and temperature data acquisition options of the device. The register needs to be writtn after changing "ctrl_hum" for the changes to become effective.
    union
    {
        struct
        {
            uint8_t osrs_h : 3; // Controls oversampling of humidity data.
            uint8_t : 5;
        };
        uint8_t val;
    } ctrl_hum; // The "ctrl_hum" register sets the humidity data acquisition options of the device. Changes to this register only become effective after a write operation to "ctrl_meas".
} bme280_config_t;

esp_err_t bme280_reset();

esp_err_t bme280_set_config(const bme280_config_t *config);
esp_err_t bme280_get_config(bme280_config_t *config);

esp_err_t bme280_force_measurement();

esp_err_t bme280_get_data(bme280_data_t *data);

esp_err_t bme280_get_chip_id(uint8_t *chip_id);