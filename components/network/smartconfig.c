#include "smartconfig.h"

#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include <string.h>

static void wifi_handler(void *handler_args, esp_event_base_t base, int event_id, void *event_data)
{
    if (event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        // handle got ssid and password event
        smartconfig_event_got_ssid_pswd_t *event = (smartconfig_event_got_ssid_pswd_t *)event_data;

        // copy the credentials from smartconfig into the wifi_config
        wifi_config_t wifi_config = {};
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
        memcpy(wifi_config.sta.ssid, event->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, event->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = event->bssid_set;
        if (wifi_config.sta.bssid_set == true)
            memcpy(wifi_config.sta.bssid, event->bssid, sizeof(wifi_config.sta.bssid));

        esp_wifi_disconnect();
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
        esp_wifi_connect();
    }
    else if (event_id == SC_EVENT_SEND_ACK_DONE)
    {
        // handle smartconfig done event
        esp_smartconfig_stop();
    }
}

esp_err_t smartconfig_start()
{
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS);
    const smartconfig_start_config_t smartcfg_config = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_fast_mode(true);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL);
    return esp_smartconfig_start(&smartcfg_config);
}