#include "mqtt_handlers.h"

#include <string.h>
#include "client_handlers/client_handlers.h"

#include "cJSON.h"

#define STATE_TOPIC "state"

#define DEFAULT_QOS 1
#define RESP_STR_LENGTH 64

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

    // build the reply topic string
    char reply_topic[strlen(r->client_base) + strlen(r->client_name) + strlen(STATE_TOPIC) + 3];
    sprintf(reply_topic, "%s/%s/%s", r->client_base, r->client_name, STATE_TOPIC);

    // send the reply back on the state topic
    char *reply = cJSON_PrintUnformatted(reply_root);
    mqtt_resp_sendstr(r, reply_topic, reply, 2, false);

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
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = "signal_strength",
        .force_update = false,
        .icon = NULL,
        .name = "Signal Strength",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_rssi",
        .unit_of_measurement = "dBm",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_SYSTEM, SYSTEM_WIFI_RSSI_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("rssi"), rssi);

#ifdef USE_MAX17043
    const discovery_string_t battery = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = "battery",
        .force_update = false,
        .icon = NULL,
        .name = "Battery Level",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_battery",
        .unit_of_measurement = "%",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_SYSTEM, SYSTEM_BATT_LIFE_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("battery"), battery);
#endif // USE_MAX17043

#ifdef USE_BME280
    const discovery_string_t temperature = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = "temperature",
        .force_update = true,
        .icon = NULL,
        .name = "Temperature",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_temperature",
        .unit_of_measurement = TEMPERATURE_SCALE,
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_BME, BME_TEMPERATURE_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("temperature"), temperature);

    const discovery_string_t humidity = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = "humidity",
        .force_update = true,
        .icon = NULL,
        .name = "Relative Humidity",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_humidity",
        .unit_of_measurement = "%",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_BME, BME_HUMIDITY_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("humidity"), humidity);

    const discovery_string_t pressure = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = "pressure",
        .force_update = true,
        .icon = NULL,
        .name = "Barometric Pressure",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_pressure",
        .unit_of_measurement = PRESSURE_SCALE,
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_BME, BME_PRESSURE_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("pressure"), pressure);

    const discovery_string_t dew_point = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = NULL,
        .force_update = true,
        .icon = "mdi:weather-fog",
        .name = "Dew Point",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_dew_point",
        .unit_of_measurement = TEMPERATURE_SCALE,
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_BME, BME_DEW_POINT_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("dew_point"), dew_point);
#endif // USE_BME280

#ifdef USE_PMS5003
    const discovery_string_t pm2_5 = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = NULL,
        .force_update = true,
        .icon = "mdi:smog",
        .name = "PM2.5",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_pm2_5",
        .unit_of_measurement = "μg/m³",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_PMS, PMS_PM2_5_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("pm2_5"), pm2_5);

    const discovery_string_t pm10 = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = NULL,
        .force_update = true,
        .icon = "mdi:smog",
        .name = "PM10",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_pm10",
        .unit_of_measurement = "μg/m³",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_PMS, PMS_PM10_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("pm10"), pm10);
#endif // USE_PMS5003

#ifdef USE_SPH0645
    const discovery_string_t avg_noise = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = NULL,
        .force_update = true,
        .icon = "mdi:volume-high",
        .name = "Average Noise Pollution",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_noise_avg",
        .unit_of_measurement = "dBc",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_SPH, SPH_AVG_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("avg"), avg_noise);

    const discovery_string_t min_noise = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = NULL,
        .force_update = true,
        .icon = "mdi:volume-minus",
        .name = "Minimum Noise Pollution",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_noise_min",
        .unit_of_measurement = "dBc",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_SPH, SPH_MIN_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("min"), min_noise);

    const discovery_string_t max_noise = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = NULL,
        .force_update = true,
        .icon = "mdi:volume-plus",
        .name = "Maximum Noise Pollution",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_noise_max",
        .unit_of_measurement = "dBc",
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_SPH, SPH_MAX_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("max"), max_noise);

    const discovery_string_t num_samples = {
        .availability_topic = MQTT_AVAILABLE_TOPIC,
        .device = DEFAULT_DEVICE,
        .device_class = NULL,
        .force_update = true,
        .icon = "mdi:counter",
        .name = "Noise Pollution Samples",
        .state_topic =  MQTT_STATE_TOPIC,
        .unique_id = MQTT_CLIENT_NAME "_noise_samples",
        .unit_of_measurement = NULL,
        .value_template = VALUE_TEMPLATE2(JSON_ROOT_SPH, SPH_NUM_SAMPLES_KEY)
    };
    mqtt_send_discovery_string(HASS_TOPIC("num_samples"), num_samples);
#endif // USE_SPH0645

    return ESP_OK;
}