#include "sensor_mgmt.h"

#include <string.h>

#include "bme280.h"
#include "cJSON.h"
#include "max17043.h"
#include "pms5003.h"
#include "sph0645.h"
#include "wireless.h"

// wifi json keys
#define JSON_SIGNAL_STRENGTH_KEY "signal_strength"
// max17043 json keys
#define JSON_BATTERY_KEY "battery"
// bme280 json keys
#define JSON_TEMPERATURE_KEY "temperature"
#define JSON_HUMIDITY_KEY "humidity"
#define JSON_PRESSURE_KEY "pressure"
#define JSON_DEW_POINT_KEY "dew_point"
// pms5003 json keys
#define JSON_PM1_KEY "pm1"
#define JSON_PM2_5_KEY "pm2_5"
#define JSON_PM10_KEY "pm10"
#define JSON_FAN_KEY "fan"
#define JSON_FAN_ON_VALUE "on"
#define JSON_FAN_OFF_VALUE "off"
// sph0645 json keys
#define JSON_AVG_NOISE_KEY "avg_noise"
#define JSON_MIN_NOISE_KEY "min_noise"
#define JSON_MAX_NOISE_KEY "max_noise"

#define STATE_TOPIC ("weather-station/" CLIENT_NAME)
#define UNIQUE_ID(n) (CLIENT_NAME "_" n)
#define VALUE_TEMPLATE(a) ("{{ value_json['" a "'] }}")

#define TRUNCATE(n) (((int64_t)(n * 100)) / 100)

#define DEFAULT_DEVICE                                             \
  {                                                                \
    .identifiers = "MODEL_NAME", .manufacturer = "Mitch Weisbrod", \
    .model = MODEL_NAME, .name = DEVICE_NAME, .sw_version = ""     \
  }

void sensors_start() {
  esp_err_t err = ESP_OK;

  const mqtt_discovery_t signal_strength = {
      .type = MQTT_SENSOR,
      .device = DEFAULT_DEVICE,
      .device_class = "signal_strength",
      .name = "Signal Strength",
      .state_topic = STATE_TOPIC,
      .unique_id = UNIQUE_ID("signal_strength"),
      .sensor =
          {
              .unit_of_measurement = SIGNAL_STRENGTH_SCALE,
          },
      .value_template = VALUE_TEMPLATE(JSON_SIGNAL_STRENGTH_KEY)};
  mqtt_publish_discovery(&signal_strength);

#ifdef USE_MAX17043
  const mqtt_discovery_t battery = {
      .type = MQTT_SENSOR,
      .device = DEFAULT_DEVICE,
      .device_class = "battery",
      .name = "Battery Level",
      .state_topic = STATE_TOPIC,
      .unique_id = UNIQUE_ID("battery"),
      .sensor =
          {
              .unit_of_measurement = BATTERY_SCALE,
          },
      .value_template = VALUE_TEMPLATE(JSON_BATTERY_KEY)};
  mqtt_publish_discovery(&battery);
#endif  // USE_MAX17043

#ifdef USE_BME280
  do {
    err = bme280_reset();
    if (err) break;
    const bme280_config_t bme_config = BME280_WEATHER_MONITORING;
    err = bme280_set_config(&bme_config);
    if (err) break;
    const double elevation = wireless_get_elevation();
    bme280_set_elevation(elevation);
  } while (false);

  const mqtt_discovery_t bme280_discovery[] = {
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .device_class = JSON_TEMPERATURE_KEY,
       .name = "Temperature",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_TEMPERATURE_KEY),
       .sensor =
           {
               .unit_of_measurement = TEMPERATURE_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_TEMPERATURE_KEY)},
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .device_class = JSON_HUMIDITY_KEY,
       .name = "Humidity",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_HUMIDITY_KEY),
       .sensor =
           {
               .unit_of_measurement = HUMIDITY_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_HUMIDITY_KEY)},
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .device_class = JSON_PRESSURE_KEY,
       .name = "Pressure",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_PRESSURE_KEY),
       .sensor =
           {
               .unit_of_measurement = PRESSURE_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_PRESSURE_KEY)},
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "Dew Point",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_DEW_POINT_KEY),
       .sensor =
           {
               .icon = "mdi:weather-fog",
               .unit_of_measurement = TEMPERATURE_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_DEW_POINT_KEY)},
  };
  for (int i = 0; i < sizeof(bme280_discovery) / sizeof(mqtt_discovery_t); ++i)
    mqtt_publish_discovery(&bme280_discovery[i]);

#endif  // USE BME280

#ifdef USE_PMS5003
  do {
    err = pms5003_reset();
    if (err) break;
    const pms5003_config_t pms_config = PMS5003_PASSIVE_ASLEEP;
    err = pms5003_set_config(&pms_config);
    if (err) break;
  } while (false);

  const mqtt_discovery_t pms5003_discovery[] = {
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "PM1",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_PM1_KEY),
       .sensor =
           {
               .icon = "mdi:smog",
               .unit_of_measurement = PM_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_PM1_KEY)},
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "PM2.5",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_PM2_5_KEY),
       .sensor =
           {
               .icon = "mdi:smog",
               .unit_of_measurement = PM_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_PM2_5_KEY)},
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "PM10",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_PM10_KEY),
       .sensor =
           {
               .icon = "mdi:smog",
               .unit_of_measurement = PM_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_PM10_KEY)},
      {.type = MQTT_BINARY_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "Air Quality Sensor Fan",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_FAN_KEY),
       .binary_sensor =
           {
               .payload_on = JSON_FAN_ON_VALUE,
               .payload_off = JSON_FAN_OFF_VALUE,
           },
       .value_template = VALUE_TEMPLATE(JSON_FAN_KEY)},
  };
  for (int i = 0; i < sizeof(pms5003_discovery) / sizeof(mqtt_discovery_t); ++i)
    mqtt_publish_discovery(&pms5003_discovery[i]);
#endif  // USE PMS_5003

#ifdef USE_SPH0645
  do {
    err = sph0645_reset();
    if (err) break;
    const sph0645_config_t sph_config = SPH0645_DEFAULT_CONFIG;
    err = sph0645_set_config(&sph_config);
    if (err) break;
  } while (false);

  const mqtt_discovery_t sph0645_discovery[] = {
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "Average Noise",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_AVG_NOISE_KEY),
       .sensor =
           {
               .icon = "mdi:volume-high",
               .unit_of_measurement = NOISE_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_AVG_NOISE_KEY)},
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "Minimum Noise",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_MIN_NOISE_KEY),
       .sensor =
           {
               .icon = "mdi:volume-minus",
               .unit_of_measurement = NOISE_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_MIN_NOISE_KEY)},
      {.type = MQTT_SENSOR,
       .device = DEFAULT_DEVICE,
       .name = "Maximum Noise",
       .state_topic = STATE_TOPIC,
       .unique_id = UNIQUE_ID(JSON_MAX_NOISE_KEY),
       .sensor =
           {
               .icon = "mdi:volume-plus",
               .unit_of_measurement = NOISE_SCALE,
           },
       .value_template = VALUE_TEMPLATE(JSON_MAX_NOISE_KEY)},
  };
  for (int i = 0; i < sizeof(sph0645_discovery) / sizeof(mqtt_discovery_t); ++i)
    mqtt_publish_discovery(&sph0645_discovery[i]);
#endif  // USE_SPH0645
}

void sensors_wakeup(cJSON* json) {
  esp_err_t err = ESP_OK;
#ifdef USE_PMS5003
  do {
    pms5003_config_t pms_config;
    err = pms5003_get_config(&pms_config);
    if (err) break;
    pms_config.sleep = 1;  // wakeup
    err = pms5003_set_config(&pms_config);
    if (err) break;
    cJSON_AddStringToObject(json, JSON_FAN_KEY, JSON_FAN_ON_VALUE);
  } while (false);
#endif  // USE_PMS5003
}

void sensors_get_data(cJSON* json) {
  esp_err_t err = ESP_OK;

  // get wifi rssi
  const int8_t rssi = wireless_get_rssi();
  cJSON_AddNumberToObject(json, JSON_SIGNAL_STRENGTH_KEY, rssi);

#ifdef USE_MAX17043
  do {
    max17043_data_t data;
    err = max17043_get_data(&data);
    if (err) break;
    cJSON_AddNumberToObject(json, JSON_BATTERY_KEY, (int)data.battery_life);
  } while (false);
#endif  // USE_MAX17043

#ifdef USE_BME280
  do {
    err = bme280_force_measurement();
    if (err) break;
    bme280_data_t data;
    err = bme280_get_data(&data);
    if (err) break;
    cJSON_AddNumberToObject(json, JSON_TEMPERATURE_KEY,
                            TRUNCATE(data.temperature));
    cJSON_AddNumberToObject(json, JSON_HUMIDITY_KEY, TRUNCATE(data.humidity));
    cJSON_AddNumberToObject(json, JSON_PRESSURE_KEY, TRUNCATE(data.pressure));
    cJSON_AddNumberToObject(json, JSON_DEW_POINT_KEY, TRUNCATE(data.dew_point));
  } while (false);
#endif  // USE BME280

#ifdef USE_PMS5003
  do {
    pms5003_data_t data;
    err = pms5003_get_data(&data);
    if (err || !data.checksum_ok) break;
    cJSON_AddNumberToObject(json, JSON_PM1_KEY, data.concAtm.pm1);
    cJSON_AddNumberToObject(json, JSON_PM2_5_KEY, data.concAtm.pm2_5);
    cJSON_AddNumberToObject(json, JSON_PM10_KEY, data.concAtm.pm10);
  } while (false);
#endif  // USE_PMS5003

#ifdef USE_SPH0645
  do {
    sph0645_data_t data;
    err = sph0645_get_data(&data);
    sph0645_reset();
    if (err) break;
    cJSON_AddNumberToObject(json, JSON_AVG_NOISE_KEY, TRUNCATE(data.avg));
    cJSON_AddNumberToObject(json, JSON_MIN_NOISE_KEY, TRUNCATE(data.min));
    cJSON_AddNumberToObject(json, JSON_MAX_NOISE_KEY, TRUNCATE(data.max));
  } while (false);
#endif  // USE_PMS5003
}

void sensors_sleep(cJSON* json) {
  esp_err_t err = ESP_OK;
#ifdef USE_PMS5003
  do {
    pms5003_config_t pms_config;
    err = pms5003_get_config(&pms_config);
    if (err) break;
    pms_config.sleep = 1;  // wakeup
    err = pms5003_set_config(&pms_config);
    if (err) break;
    cJSON_AddStringToObject(json, JSON_FAN_KEY, JSON_FAN_OFF_VALUE);
  } while (false);
#endif  // USE_PMS5003
}