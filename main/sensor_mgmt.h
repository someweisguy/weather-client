#pragma once
#include "cJSON.h"
#include "esp_system.h"

// which sensors to use and names
#ifdef CONFIG_OUTSIDE_STATION
#define DEVICE_NAME "Outdoor Weather Station"
#define MODEL_NAME "Outdoor Model"
#define CLIENT_NAME "outside"
#define USE_MAX17043
#define USE_BME280
#define USE_PMS5003
#define USE_SPH0645
#elif defined(CONFIG_INSIDE_STATION)
#define DEVICE_NAME "Indoor Weather Station"
#define MODEL_NAME "Indoor Model"
#define CLIENT_NAME "inside"
#define USE_BME280
#define USE_PMS5003
#define USE_SPH0645
// TODO: add CO2 sensor
#elif defined(CONFIG_WIND_AND_RAIN_STATION)
#define DEVICE_NAME "Wind & Rain Station"
#define MODEL_NAME "Wind & Rain Model"
#define CLIENT_NAME "wind_rain"
#define USE_MAX17043
// TODO: add wind vane and rain gauge
#endif

// sensor measurement scales
#ifdef CONFIG_CELSIUS
#define TEMPERATURE_SCALE "°C"
#elif defined(CONFIG_FAHRENHEIT)
#define TEMPERATURE_SCALE "°F"
#elif defined(CONFIG_KELVIN)
#define TEMPERATURE_SCALE "K"
#endif
#ifdef CONFIG_MM_HG
#define PRESSURE_SCALE "mmHg"
#elif defined(CONFIG_IN_HG)
#define PRESSURE_SCALE "inHg"
#endif
#define SIGNAL_STRENGTH_SCALE "dB"
#define BATTERY_SCALE "%"
#define HUMIDITY_SCALE "%"
#define NOISE_SCALE "dBc"
#define PM_SCALE "μg/m³"

void sensors_start();

void sensors_wakeup(cJSON *json);

void sensors_get_data(cJSON *json);

void sensors_sleep(cJSON *json);