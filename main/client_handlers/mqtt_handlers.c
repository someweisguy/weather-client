#include "mqtt_handlers.h"

#include "client_handlers/client_handlers.h"

#include "cJSON.h"

#define DEFAULT_QOS 1
#define ERROR_RESPONSE(s) ("{\n\t\"error\":\t" s "\n}")

static const char* MQTT_RESP_SUFFIX = "/res";

esp_err_t mqtt_about_handler(mqtt_req_t *r)
{
    // build the response topic
    char resp_topic[strlen(MQTT_RESP_SUFFIX) + strlen(r->topic) + 1];
    strcpy(resp_topic, r->topic);
    strcat(resp_topic, MQTT_RESP_SUFFIX);

    // send the response
    char *response = about_handler();
    mqtt_resp_sendstr(r, resp_topic, response, DEFAULT_QOS, false);

    free(response);

    return ESP_OK;
}

esp_err_t mqtt_config_handler(mqtt_req_t *r)
{
    // build the response topic
    char resp_topic[strlen(MQTT_RESP_SUFFIX) + strlen(r->topic) + 1];
    strcpy(resp_topic, r->topic);
    strcat(resp_topic, MQTT_RESP_SUFFIX);

    // read the body of the request into memory
    char *request = malloc(r->content_len + 1); // content + null terminator
    if (request == NULL)
    {
        mqtt_resp_sendstr(r, resp_topic, ERROR_RESPONSE("NO MEMORY"), DEFAULT_QOS, false);
        return ESP_ERR_NO_MEM;
    }
    memcpy(request, r->content, r->content_len);

    // send a response to the client
    esp_err_t err = config_handler(request);
    mqtt_resp_sendstr(r, resp_topic, esp_err_to_name(err), DEFAULT_QOS, false);

    free(request);

    return err;
}

esp_err_t mqtt_data_handler(mqtt_req_t *r)
{
    // build the response topic
    char resp_topic[strlen(MQTT_RESP_SUFFIX) + strlen(r->topic) + 1];
    strcpy(resp_topic, r->topic);
    strcat(resp_topic, MQTT_RESP_SUFFIX);

    // read the body of the request into memory
    char *request = malloc(r->content_len + 1); // content + null terminator
    if (request == NULL)
    {
        mqtt_resp_sendstr(r, resp_topic, ERROR_RESPONSE("NO MEMORY"), DEFAULT_QOS, false);
        return ESP_ERR_NO_MEM;
    }
    memcpy(request, r->content, r->content_len);

    bool clear_data = false;
    cJSON *root = cJSON_Parse(request);
    if (root != NULL)
    {
        const cJSON *clear_data_node = cJSON_GetObjectItem(root, SPH_CLEAR_DATA_KEY);
        if (clear_data_node != NULL && cJSON_IsTrue(clear_data_node))
            clear_data = true;
    }
    
    // send the response to the client
    char *response = data_handler(clear_data);    
    mqtt_resp_sendstr(r, resp_topic, response, DEFAULT_QOS, false);

    free(response);

    return ESP_OK;
}