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
    return ESP_OK;
}