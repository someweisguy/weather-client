#include "sensor_mgmt.h"

#include "cJSON.h"

#include "bme280.h"
#include "max17043.h"
#include "pms5003.h"
#include "sph0645.h"
#include "wireless.h"

#ifdef CONFIG_OUTSIDE_STATION
#define USE_MAX17043
#define USE_BME280
#define USE_PMS5003
#define USE_SPH0645
#elif defined(CONFIG_INSIDE_STATION)
#define USE_BME280
#define USE_PMS5003
#define USE_SPH0645
// TODO: add CO2 sensor
#elif defined(CONFIG_WIND_AND_RAIN_STATION)
#define USE_MAX17043
// TODO: add wind vane and rain gauge
#endif

// wifi json keys
#define JSON_RSSI_KEY           "signal_strength"
// max17043 json keys
#define JSON_BATT_KEY           "battery"
// bme280 json keys
#define JSON_TEMPERATURE_KEY    "temperature"
#define JSON_HUMIDITY_KEY       "humidity"
#define JSON_PRESSURE_KEY       "pressure"
#define JSON_DEW_POINT_KEY      "dew_point"
// pms5003 json keys
#define JSON_PM1_KEY            "pm1"
#define JSON_PM2_5_KEY          "pm2_5"
#define JSON_PM10_KEY           "pm10"
#define JSON_FAN_KEY            "fan"
// sph0645 json keys
#define JSON_AVG_NOISE_KEY      "avg_noise"
#define JSON_MIN_NOISE_KEY      "min_noise"
#define JSON_MAX_NOISE_KEY      "max_noise"

void sensors_start(cJSON *json)
{
    esp_err_t err = ESP_OK;
#ifdef USE_BME280
    do
    {
        err = bme280_reset();
        if (err)
            break;
        const bme280_config_t bme_config = BME280_WEATHER_MONITORING;
        err = bme280_set_config(&bme_config);
        if (err)
            break;
        const double elevation = wireless_get_elevation();
        bme280_set_elevation(elevation);
    } while (false);
#endif // USE BME280

#ifdef USE_PMS5003
    do
    {
        err = pms5003_reset();
        if (err)
            break;
        const pms5003_config_t pms_config = PMS5003_PASSIVE_ASLEEP;
        err = pms5003_set_config(&pms_config);
        if (err)
            break;
    } while (false);
#endif // USE PMS_5003

#ifdef USE_SPH0645
    do
    {
        err = sph0645_reset();
        if (err)
            break;
        const sph0645_config_t sph_config = SPH0645_DEFAULT_CONFIG;
        err = sph0645_set_config(&sph_config);
        if (err)
            break;
    } while (false);
#endif // USE_SPH0645
}

void sensors_wakeup(cJSON *json)
{
    esp_err_t err = ESP_OK;
#ifdef USE_PMS5003
    do
    {
        pms5003_config_t pms_config;
        err = pms5003_get_config(&pms_config);
        if (err)
            break;
        pms_config.sleep = 1; // wakeup
        err = pms5003_set_config(&pms_config);
        if (err)
            break;
    } while (false);
#endif // USE_PMS5003
}

void sensors_get_data(cJSON *json)
{
    esp_err_t err = ESP_OK;

    // get wifi rssi
    const int8_t rssi = wireless_get_rssi();
    cJSON_AddNumberToObject(json, JSON_RSSI_KEY, rssi);

#ifdef USE_MAX17043
    do
    {
        max17043_data_t data;
        err = max17043_get_data(&data);
        if (err)
            break;
        const uint8_t battery = (uint8_t)data.battery_life;
        cJSON_AddNumberToObject(json, JSON_BATT_KEY, battery);
    } while (false);
#endif // USE_MAX17043
}

void sensors_sleep(cJSON *json)
{
    esp_err_t err = ESP_OK;
#ifdef USE_PMS5003
    do
    {
        pms5003_config_t pms_config;
        err = pms5003_get_config(&pms_config);
        if (err)
            break;
        pms_config.sleep = 1; // wakeup
        err = pms5003_set_config(&pms_config);
        if (err)
            break;
    } while (false);
#endif // USE_PMS5003
}