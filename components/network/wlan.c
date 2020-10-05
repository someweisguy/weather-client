#include "wlan.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "uploader.h"
#include "esp_smartconfig.h"

static volatile uint8_t retries = 0;
static int64_t up_time = -1;

static void event_handler(void *handler_args, esp_event_base_t base,
                          int event_id, void *event_data)
{
    // handle wifi events
    if (base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            // Do nothing
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            up_time = -1;
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_STOP:
            // Do nothing
            break;

        default:
            ESP_LOGW("wlan", "unexpected event %i", event_id);
            break;
        }
    }

    // handle smartconfig event
    else if (base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
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
    else if (base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
        esp_smartconfig_stop();

    // handle got ip event
    else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        up_time = esp_timer_get_time();
}

esp_err_t wlan_start()
{
    // init network interface and wifi sta
    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    // init wifi driver
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);

    // register wifi event handlers
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);

    // get wifi credentials from uploader or nvs
    wifi_config_t wifi_config = {};
    if (uploader_get_config(&wifi_config, 100 / portTICK_PERIOD_MS) != ESP_OK &&
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK)
        return ESP_FAIL;

    // configure and start wifi
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

    // configure and start smartconfig
    if (strlen((const char *)(wifi_config.sta.ssid)) == 0)
    {
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS);
        smartconfig_start_config_t smartcfg_config = SMARTCONFIG_START_CONFIG_DEFAULT();
        esp_smartconfig_start(&smartcfg_config);
    }

    return ESP_OK;
}

esp_err_t wlan_stop()
{
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler);
    esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, event_handler);

    esp_smartconfig_stop();

    esp_wifi_stop();

    vTaskDelay(500 / portTICK_PERIOD_MS); // wait for wifi to stop

    esp_wifi_deinit();
    return ESP_OK;
}

esp_err_t wlan_get_data(wlan_data_t *data)
{
    esp_netif_t *netif = NULL;
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i)
    {
        // iterate the netifs to find which one is active
        netif = esp_netif_next(netif);
        if (esp_netif_is_netif_up(netif))
            break;
    }

    // Get the IP address
    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(netif, &ip_info);
    if (err)
        return err;
    data->ip = ip_info.ip;

    // copy the ip as a string
    sprintf(data->ip_str, "%d.%d.%d.%d", IP2STR(&ip_info.ip));

    // Get the RSSI
    wifi_ap_record_t ap_info;
    err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err)
        ap_info.rssi = 0;
    data->rssi = ap_info.rssi;

    // copy the wifi uptime
    data->up_time = (esp_timer_get_time() - up_time) / 1000;

    return ESP_OK;
}