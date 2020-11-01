#include "wireless.h"

#include "esp_sntp.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "smartconfig.h"
#include <string.h>

typedef struct
{
    char *mqtt_broker;
    sntp_sync_time_cb_t callback;
} connect_args_t;

static esp_mqtt_client_handle_t mqtt_client = NULL;

static esp_err_t mqtt_handler(esp_mqtt_event_handle_t event)
{
    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        //mqtt_send("online", "Hello world!", 2, 0);
    }
    else
    {
        printf("Other MQTT event occurred: %i\n", event->event_id);
    }
    return ESP_OK;
}

static void wifi_handler(void *handler_args, esp_event_base_t base, int event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // handle sta start
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // handle disconnect
        const wifi_event_sta_disconnected_t *wifi_data = event_data;

        // if bad password start smart config
        if (wifi_data->reason == WIFI_REASON_AUTH_FAIL)
            smartconfig_start();
        else
            esp_wifi_connect();

        // stop the mqtt client if it is running
        if (mqtt_client != NULL)
            esp_mqtt_client_stop(mqtt_client);
    }
    else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // handle connect
        if (mqtt_client == NULL)
        {
            // this is the first time we're connecting
            // get the connect args
            connect_args_t *connect_args = (connect_args_t *)handler_args;

            // start the mqtt client
            const esp_mqtt_client_config_t config = {
                .uri = connect_args->mqtt_broker,
                .event_handle = mqtt_handler};
            mqtt_client = esp_mqtt_client_init(&config);

            // connect to the sntp server and synchronize the time
            if (connect_args->callback != NULL)
                sntp_set_time_sync_notification_cb(connect_args->callback);
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0, "pool.ntp.org");
            sntp_set_sync_interval(60 * 1000); // 1 minute
            sntp_init();
        }

        esp_mqtt_client_start(mqtt_client);
    }
}

esp_err_t wireless_start(const char *mqtt_broker, sntp_sync_time_cb_t callback)
{
    // init network interface and wifi sta
    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    // init wifi driver
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);

    // allocate connect args
    connect_args_t *connect_args = malloc(sizeof(connect_args_t));
    connect_args->mqtt_broker = strdup(mqtt_broker);
    connect_args->callback = callback;

    // register wifi event handlers
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, (void *)connect_args);

    // get wifi credentials from nvs
    wifi_config_t wifi_config = {};
    esp_err_t err = esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
    if (err)
        return err;

    // check that an ssid was found
    if (strlen((const char *)(wifi_config.sta.ssid)) > 0)
    {
        // configure and start wifi
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
        esp_wifi_start();
    }
    else
        // start smartconfig
        smartconfig_start();

    return ESP_OK;
}

esp_err_t mqtt_send(const char *topic, const char *message, int qos, bool retain)
{
    if (mqtt_client == NULL)
        return ESP_ERR_INVALID_STATE;
    const int ret = esp_mqtt_client_publish(mqtt_client, topic, message,
                                            0, qos, retain);
    return ret == -1 ? ESP_FAIL : ESP_OK;
}