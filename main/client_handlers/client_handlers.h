#pragma once

#include "esp_system.h"
#include "cJSON.h"

#define JSON_DATA_KEY "data"
#define JSON_CONFIG_KEY "config"

#define JSON_ROOT_SYSTEM "system"
#define SYSTEM_WIFI_UP_TIME_KEY "connection_time"
#define SYSTEM_WIFI_RSSI_KEY "rssi"
#define SYSTEM_IP_KEY "ip"
#define SYSTEM_BATT_LIFE_KEY "battery"
#define SYSTEM_BATT_VOLTAGE_KEY "millivolts"

#define JSON_ROOT_BME "climate"
#define BME_TEMPERATURE_KEY "temperature"
#define BME_HUMIDITY_KEY "humidity"
#define BME_PRESSURE_KEY "pressure"
#define BME_DEW_POINT_KEY "dew_point"
#define BME_OVERSAMPLING_KEY "oversampling"
#define BME_MODE_KEY "mode"
#define BME_STANDBY_TIME_KEY "standby_time"
#define BME_FILTER_KEY "filter"
#define BME_ELEVATION_KEY "elevation"

#define JSON_ROOT_PMS "smoke"
#define PMS_FAN_UP_TIME_KEY "fan_time"
#define PMS_CHECKSUM_OK_KEY "checksum_ok"
#define PMS_STANDARD_PARTICLE_KEY "standard"
#define PMS_ATMOSPHERIC_PARTICLE_KEY "atmospheric"
#define PMS_FAN_KEY "fan"
#define PMS_PM1_KEY "pm1"
#define PMS_PM2_5_KEY "pm2_5"
#define PMS_PM10_KEY "pm10"
#define PMS_COUNT_PER_0_1L_KEY "count_per_0_1L"
#define PMS_0_3UM_KEY "0_3um"
#define PMS_0_5UM_KEY "0_5um"
#define PMS_1_0UM_KEY "1_0um"
#define PMS_2_5UM_KEY "2_5um"
#define PMS_5_0UM_KEY "5_0um"
#define PMS_10_0UM_KEY "10_0um"
#define PMS_MODE_KEY "mode"
#define PMS_SLEEP_SET_KEY "awake"

#define JSON_ROOT_SPH "noise"
#define SPH_AVG_KEY "avg"
#define SPH_MIN_KEY "min"
#define SPH_MAX_KEY "max"
#define SPH_NUM_SAMPLES_KEY "num_samples"
#define SPH_SAMPLE_LEN_KEY "sample_len"
#define SPH_SAMPLE_PERIOD_KEY "sample_period"
#define SPH_SAMPLE_WEIGHTING_KEY "weighting"
#define SPH_CLEAR_DATA_KEY "clear_data"


#define MQTT_STATUS_KEY "status"
#define MQTT_ON_STATUS "on"
#define MQTT_OFF_STATUS "off"

#define MQTT_GET_DATA_KEY "get_data"
#define MQTT_RESET_DATA_KEY "reset_data"

#ifdef CONFIG_OUTSIDE_STATION
#define USE_MAX17043
#define USE_BME280
#define USE_PMS5003
#define USE_SPH0645
#endif

#ifdef CONFIG_INSIDE_STATION
#define USE_BME280
#define USE_PMS5003
// TODO: add CO2 sensor
#endif

#ifdef CONFIG_WIND_AND_RAIN_STATION
#define USE_MAX17043
// TODO: add wind vane and rain gauge
#endif

char *about_handler();

esp_err_t config_handler(const char *request);

char *data_handler(const bool clear_data);

void restart_handler();

esp_err_t sensors_set_status(cJSON *root, bool awake);

esp_err_t sensors_get_data(cJSON *root);

esp_err_t sensors_clear_data(cJSON *root);