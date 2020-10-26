#include "mqtt_handlers.h"

#include <string.h>
#include "client_handlers/client_handlers.h"
#include "cJSON.h"

// device and model names
#ifdef CONFIG_OUTSIDE_STATION
#define DEVICE_NAME             "Outdoor Weather Station"
#define MODEL_NAME              "Outdoor Model"
#elif defined(CONFIG_INSIDE_STATION)
#define DEVICE_NAME             "Indoor Weather Station"
#define MODEL_NAME              "Indoor Model"
#elif defined(CONFIG_WIND_AND_RAIN_STATION)
#define DEVICE_NAME             "Wind & Rain Station"
#define MODEL_NAME              "Wind & Rain Model"
#endif

// sensor measurement scales
#ifdef CONFIG_CELSIUS
#define TEMPERATURE_SCALE       "°C"
#elif defined(CONFIG_FAHRENHEIT)
#define TEMPERATURE_SCALE       "°F"
#elif defined(CONFIG_KELVIN)
#define TEMPERATURE_SCALE       "K"
#endif
#ifdef CONFIG_MM_HG
#define PRESSURE_SCALE          "mmHg"
#elif defined(CONFIG_IN_HG)
#define PRESSURE_SCALE          "inHg"
#endif
#define SIGNAL_STRENGTH_SCALE   "dB"
#define BATTERY_SCALE           "%"
#define HUMIDITY_SCALE          "%"
#define VOLUME_SCALE            "dBc"
#define PM_SCALE                "μg/m³"

#define EXPIRE_AFTER            5 * 60

#define UNIQUE_ID(n)            (MQTT_CLIENT_NAME "_" n)
#define SENSOR_TOPIC(n)         ("homeassistant/sensor/" MQTT_CLIENT_NAME "_" n "/config")
#define BINARY_SENSOR_TOPIC(n)  ("homeassistant/binary_sensor/" MQTT_CLIENT_NAME "_" n "/config")
#define VALUE_TEMPLATE(a, b)    ("{{ value_json[\"" a "\"][\"" b "\"] }}")

#define DEFAULT_DEVICE                      \
    {                                       \
        .name = DEVICE_NAME,                \
        .manufacturer = "Mitch Weisbrod",   \
        .model = MODEL_NAME,                \
        .sw_version = "",                   \
        .identifiers = MQTT_CLIENT_NAME     \
    }

esp_err_t mqtt_request_handler(mqtt_req_t *r)
{
    // get the request
    char *request = malloc(r->content_len + 1);
    if (request == NULL)
        return ESP_ERR_NO_MEM;
    strncpy(request, r->content, r->content_len);

    // parse the request
    cJSON *req_root = cJSON_Parse(request);
    if (req_root == NULL)
    {
        free(request);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *reply_root = cJSON_CreateObject();

    // handle getting the data
    cJSON *get_data_node = cJSON_GetObjectItem(req_root, MQTT_GET_DATA_KEY);
    if (get_data_node != NULL && cJSON_IsTrue(get_data_node))
        sensors_get_data(reply_root);

    // handle clearing the data
    cJSON *clear_data_node = cJSON_GetObjectItem(req_root, MQTT_RESET_DATA_KEY);
    if (clear_data_node != NULL && cJSON_IsTrue(clear_data_node))
        sensors_clear_data(reply_root);

    // handle waking and sleeping the client
    cJSON *wake_node = cJSON_GetObjectItem(req_root, MQTT_STATUS_KEY);
    if (wake_node != NULL)
    {
        if (strcasecmp(wake_node->valuestring, MQTT_AWAKE_STATUS) == 0)
            sensors_set_status(reply_root, true); // wakeup the client
        else if (strcasecmp(wake_node->valuestring, MQTT_ASLEEP_STATUS) == 0)
            sensors_set_status(reply_root, false); // sleep the client
    }

    // send the reply back on the state topic
    char *reply = cJSON_PrintUnformatted(reply_root);
    mqtt_resp_sendstr(r, MQTT_STATE_TOPIC, reply, 2, false);

    // free resources
    cJSON_Delete(reply_root);
    cJSON_Delete(req_root);
    free(request);
    free(reply);

    return ESP_OK;
}

esp_err_t mqtt_homeassistant_handler(mqtt_req_t *r)
{
    const discovery_string_t rssi = {
        .device = DEFAULT_DEVICE,
        .device_class = "signal_strength",
        .force_update = false,
        .name = "Signal Strength",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("rssi"),
        .unit_of_measurement = SIGNAL_STRENGTH_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_SYSTEM, SYSTEM_WIFI_RSSI_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("rssi"), rssi);

#ifdef USE_MAX17043
    const discovery_string_t battery = {
        .device = DEFAULT_DEVICE,
        .device_class = "battery",
        .force_update = false,
        .name = "Battery Level",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("battery"),
        .unit_of_measurement = BATTERY_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_SYSTEM, SYSTEM_BATT_LIFE_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("battery"), battery);
#endif // USE_MAX17043

#ifdef USE_BME280
    const discovery_string_t temperature = {
        .device = DEFAULT_DEVICE,
        .device_class = "temperature",
        .force_update = true,
        .name = "Temperature",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("temperature"),
        .unit_of_measurement = TEMPERATURE_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_BME, BME_TEMPERATURE_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("temperature"), temperature);

    const discovery_string_t humidity = {
        .device = DEFAULT_DEVICE,
        .device_class = "humidity",
        .force_update = true,
        .name = "Relative Humidity",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("humidity"),
        .unit_of_measurement = HUMIDITY_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_BME, BME_HUMIDITY_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("humidity"), humidity);

    const discovery_string_t pressure = {
        .device = DEFAULT_DEVICE,
        .device_class = "pressure",
        .force_update = true,
        .name = "Barometric Pressure",
        .state_topic = MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("pressure"),
        .unit_of_measurement = PRESSURE_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_BME, BME_PRESSURE_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("pressure"), pressure);

    const discovery_string_t dew_point = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:weather-fog",
        .name = "Dew Point",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("dew_point"),
        .unit_of_measurement = TEMPERATURE_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_BME, BME_DEW_POINT_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("dew_point"), dew_point);
#endif // USE_BME280

#ifdef USE_PMS5003
    const discovery_string_t fan = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:fan",
        .name = "Air Quality Sensor",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("fan"),
        .value_template = VALUE_TEMPLATE(JSON_ROOT_PMS, PMS_FAN_KEY)
    };
    mqtt_send_discovery_string(BINARY_SENSOR_TOPIC("fan"), fan);

    const discovery_string_t pm1 = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:smog",
        .name = "PM1",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("pm1"),
        .unit_of_measurement = PM_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_PMS, PMS_PM1_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("pm1"), pm1);

    const discovery_string_t pm2_5 = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:smog",
        .name = "PM2.5",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("pm2_5"),
        .unit_of_measurement = PM_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_PMS, PMS_PM2_5_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("pm2_5"), pm2_5);

    const discovery_string_t pm10 = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:smog",
        .name = "PM10",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("pm10"),
        .unit_of_measurement = PM_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_PMS, PMS_PM10_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("pm10"), pm10);
#endif // USE_PMS5003

#ifdef USE_SPH0645
    const discovery_string_t avg_noise = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:volume-high",
        .name = "Average Noise Pollution",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("avg_noise"),
        .unit_of_measurement = VOLUME_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_SPH, SPH_AVG_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("avg_vol"), avg_noise);

    const discovery_string_t min_noise = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:volume-minus",
        .name = "Minimum Noise Pollution",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("min_noise"),
        .unit_of_measurement = VOLUME_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_SPH, SPH_MIN_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("min_vol"), min_noise);

    const discovery_string_t max_noise = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:volume-plus",
        .name = "Maximum Noise Pollution",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("max_noise"),
        .unit_of_measurement = VOLUME_SCALE,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_SPH, SPH_MAX_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("max_vol"), max_noise);

    const discovery_string_t num_noise_samples = {
        .device = DEFAULT_DEVICE,
        .force_update = true,
        .icon = "mdi:counter",
        .name = "Noise Pollution Samples",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = UNIQUE_ID("noise_samples"),
        .unit_of_measurement = NULL,
        .value_template = VALUE_TEMPLATE(JSON_ROOT_SPH, SPH_NUM_SAMPLES_KEY)
    };
    mqtt_send_discovery_string(SENSOR_TOPIC("num_vol_samples"), num_noise_samples);
#endif // USE_SPH0645

    return ESP_OK;
}