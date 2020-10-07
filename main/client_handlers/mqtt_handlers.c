#include "mqtt_handlers.h"

#include "client_handlers/client_handlers.h"

static const char* MQTT_RESP_SUFFIX = "/res";

esp_err_t mqtt_about_handler(mqtt_req_t *r)
{
    // build the response topic
    char resp_topic[strlen(MQTT_RESP_SUFFIX) + strlen(r->topic) + 1];
    strcpy(resp_topic, r->topic);
    strcat(resp_topic, MQTT_RESP_SUFFIX);

    // send the response
    const char *response = about_handler();
    mqtt_resp_sendstr(r, resp_topic, response, 1, false);

    free(response);

    return ESP_OK;
}