#include "wlan.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "uploader.h"

static const char *TAG = "wlan";
static volatile uint8_t retries = 0;

static void event_handler(void *handler_args, esp_event_base_t base,
                          int event_id, void *event_data)
{
    // handle wifi events
    if (base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Connecting to WiFi...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            // Do nothing
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Reconnecting...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_STOP:
            // Do nothing
            break;

        default:
            ESP_LOGW(TAG, "unexpected event %i", event_id);
            break;
        }
    }

    // handle got ip event
    else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        const ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got IP '%d.%d.%d.%d'", IP2STR(&event->ip_info.ip));
    }
}

esp_err_t wifi_start()
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

    // get wifi credentials from uploader or nvs
    wifi_config_t wifi_config = {};
    if (uploader_get_config(&wifi_config, 100 / portTICK_PERIOD_MS) != ESP_OK &&
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK)
        return ESP_FAIL;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    return ESP_OK;
}

esp_err_t wifi_stop()
{
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler);
    esp_wifi_stop();

    vTaskDelay(500 / portTICK_PERIOD_MS); // wait for wifi to stop

    esp_wifi_deinit();
    return ESP_OK;
}

int8_t wifi_get_rssi()
{
    wifi_ap_record_t wifi_record;
    esp_err_t err = esp_wifi_sta_get_ap_info(&wifi_record);
    if (err)
        return 0;
    return wifi_record.rssi;
}