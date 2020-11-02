#include "sensor_mgmt.h"

#include "bme280.h"
#include "cJSON.h"
#include "max17043.h"
#include "pms5003.h"
#include "sph0645.h"
#include "wireless.h"

// wifi json keys
#define JSON_RSSI_KEY "signal_strength"
// max17043 json keys
#define JSON_BATT_KEY "battery"
// bme280 json keys
#define JSON_TEMPERATURE_KEY "temperature"
#define JSON_HUMIDITY_KEY "humidity"
#define JSON_PRESSURE_KEY "pressure"
#define JSON_DEW_POINT_KEY "dew_point"
#define JSON_ELEVATION_KEY "elevation"
// pms5003 json keys
#define JSON_PM1_KEY "pm1"
#define JSON_PM2_5_KEY "pm2_5"
#define JSON_PM10_KEY "pm10"
#define JSON_FAN_KEY "fan"
// sph0645 json keys
#define JSON_AVG_NOISE_KEY "avg_noise"
#define JSON_MIN_NOISE_KEY "min_noise"
#define JSON_MAX_NOISE_KEY "max_noise"

#define STATE_TOPIC ("weather-station/" CLIENT_NAME)
#define UNIQUE_ID(n) (CLIENT_NAME "_" n)
#define SENSOR_TOPIC(n) ("homeassistant/sensor/" CLIENT_NAME "_" n "/config")
#define BINARY_SENSOR_TOPIC(n) ("homeassistant/binary_sensor/" CLIENT_NAME "_" n "/config")
#define VALUE_TEMPLATE(a) ("{{ value_json['" a "'] }}")

#define TRUNCATE(n) (((int64_t)(n * 100)) / 100)

#define DEFAULT_DEVICE                    \
    {                                     \
        .identifiers = MODEL_NAME,        \
        .manufacturer = "Mitch Weisbrod", \
        .model = MODEL_NAME,              \
        .name = DEVICE_NAME,              \
        .sw_version = ""                  \
    }

void sensors_start()
{
    esp_err_t err = ESP_OK;

    const mqtt_discovery_t signal_strength = {
        .type = MQTT_SENSOR,
        .device = DEFAULT_DEVICE,
        .device_class = "signal_strength",
        .name = "Signal Strength",
        .state_topic = STATE_TOPIC,
        .unique_id = UNIQUE_ID("signal_strength"),
        .sensor = {
            .unit_of_measurement = SIGNAL_STRENGTH_SCALE,
        },
        .value_template = VALUE_TEMPLATE(JSON_RSSI_KEY)};
    mqtt_publish_discovery(SENSOR_TOPIC("signal_strength"), signal_strength);

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

#ifdef USE_BME280
    do
    {
        bme280_data_t data;
        err = bme280_force_measurement();
        if (err)
            break;
        err = bme280_get_data(&data);
        if (err)
            break;
        cJSON_AddNumberToObject(json, JSON_TEMPERATURE_KEY, TRUNCATE(data.temperature));
        cJSON_AddNumberToObject(json, JSON_HUMIDITY_KEY, TRUNCATE(data.humidity));
        cJSON_AddNumberToObject(json, JSON_PRESSURE_KEY, TRUNCATE(data.pressure));
        cJSON_AddNumberToObject(json, JSON_DEW_POINT_KEY, TRUNCATE(data.dew_point));
        cJSON_AddNumberToObject(json, JSON_ELEVATION_KEY, TRUNCATE(bme280_get_elevation()));
    } while (false);
#endif // USE BME280

#ifdef USE_PMS5003

#endif // USE_PMS5003

#ifdef USE_SPH0645

#endif // USE_PMS5003
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