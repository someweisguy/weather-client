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

static char *get_discovery_string(const char *availability_topic,
                                  const char *device_class, const bool force_update, const char *icon,
                                  const char *name, const char *state_topic, const char *unique_id,
                                  const char *unit_of_measurement, const char *value_template)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "availability_topic", availability_topic);
    cJSON *device = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(device, "name", MQTT_DEVICE_NAME);
    cJSON_AddStringToObject(device, "manufacturer", "Mitch Weisbrod");
    cJSON_AddStringToObject(device, "model", MQTT_MODEL_NAME);
    cJSON_AddStringToObject(device, "sw_version", "");
    cJSON_AddStringToObject(device, "identifiers", __DATE__ __TIME__);
    if (device_class != NULL)
        cJSON_AddStringToObject(root, "device_class", device_class);
    cJSON_AddBoolToObject(root, "force_update", force_update);
    if (icon != NULL)
        cJSON_AddStringToObject(root, "icon", icon);
    cJSON_AddStringToObject(root, "name", name);
    cJSON_AddStringToObject(root, "state_topic", state_topic);
    cJSON_AddStringToObject(root, "unique_id", unique_id);
    cJSON_AddStringToObject(root, "unit_of_measurement", unit_of_measurement);
    cJSON_AddStringToObject(root, "value_template", value_template);

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return str;
}

esp_err_t mqtt_homeassistant_handler(mqtt_req_t *r)
{
    char *str;

#ifdef USE_BME280

#ifdef CONFIG_CELSIUS
    const char *degrees = "°C";
#elif defined(CONFIG_FAHRENHEIT)
    const char *degrees = "°F";
#elif defined(CONFIG_KELVIN)
    const char *degrees = "K";
#endif
#ifdef CONFIG_MM_HG
    const char *pressure = "mmHg";
#elif defined(CONFIG_IN_HG)
    const char *pressure = "inHg";
#endif

    // temperature
    str = get_discovery_string(MQTT_AVAILABLE_TOPIC,
                               "temperature",
                               true,
                               NULL,
                               MQTT_CLIENT_NAME_UPPER " Temperature",
                               MQTT_STATE_TOPIC,
                               MQTT_CLIENT_NAME "_temperature",
                               degrees,
                               "{{ value_json[\"" JSON_ROOT_BME "\"][\"" BME_TEMPERATURE_KEY "\"] }}");
    mqtt_resp_sendstr(r, HASS_TOPIC("temperature"), str, 2, true);
    free(str);

    // humidity
    str = get_discovery_string(MQTT_AVAILABLE_TOPIC,
                               "humidity",
                               true,
                               NULL,
                               MQTT_CLIENT_NAME_UPPER " Relative Humidity",
                               MQTT_STATE_TOPIC,
                               MQTT_CLIENT_NAME "_humidity",
                               "%",
                               "{{ value_json[\"" JSON_ROOT_BME "\"][\"" BME_HUMIDITY_KEY "\"] }}");
    mqtt_resp_sendstr(r, HASS_TOPIC("humidity"), str, 2, true);
    free(str);

    // pressure
    str = get_discovery_string(MQTT_AVAILABLE_TOPIC,
                               "pressure",
                               true,
                               NULL,
                               "Barometric Pressure",
                               MQTT_STATE_TOPIC,
                               MQTT_CLIENT_NAME "_pressure",
                               pressure,
                               "{{ value_json[\"" JSON_ROOT_BME "\"][\"" BME_PRESSURE_KEY "\"] }}");
    mqtt_resp_sendstr(r, HASS_TOPIC("pressure"), str, 2, true);
    free(str);

#endif // USE_BME280

    return ESP_OK;
}