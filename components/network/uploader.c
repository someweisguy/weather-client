#include "uploader.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

#include "driver/uart.h"
#include "cJSON.h"

#define PREFIX "UPLOADER"
#define JSON_READY PREFIX " JSON_READY "
#define JSON_OK PREFIX " JSON_OK "
#define JSON_BAD_PAYLOAD PREFIX " JSON_BAD_PAYLOAD "
#define JSON_UNEXPECTED_EVENT PREFIX " JSON_UNEXPECTED_EVENT "
#define WIFI_READY PREFIX " WIFI_READY "
#define WIFI_OK PREFIX " WIFI_OK "
#define WIFI_FAIL PREFIX " WIFI_FAIL "

static const char *TAG = "uploader";
static QueueHandle_t uart0_queue;
static volatile uint8_t connect_attempts;

static void response_handler(void *handler_args, esp_event_base_t base,
                             int event_id, void *event_data)
{
    if (base == WIFI_EVENT)
    {

        if (event_id == WIFI_EVENT_STA_START)
            puts(WIFI_READY);
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            if (connect_attempts++ >= 10)
            {
                esp_wifi_stop();
            }
        }
        else if (event_id == WIFI_EVENT_STA_STOP)
        {
            puts(WIFI_FAIL);
            esp_event_handler_unregister(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID,
                                         response_handler);
        }
    }
    else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // send the IP address to the uploader
        const ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        printf(WIFI_OK "%d.%d.%d.%d\n", IP2STR(&event->ip_info.ip));
        esp_event_handler_unregister(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID,
                                     response_handler);
    }
}

esp_err_t uploader_get_config(wifi_config_t *wifi_config, const TickType_t timeout)
{
    esp_err_t ret = ESP_OK;
    if (!uart_is_driver_installed(UART_NUM_0))
    {
        // install the driver to handle uart events
        ESP_LOGI(TAG, "installing uploader uart driver");
        ret = uart_driver_install(UART_NUM_0, 1024, 0, 20, &uart0_queue, 0);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "uart_driver_install error: %d", ret);
            return ret;
        }
    }

    // send ready signal to the uploader
    puts(JSON_READY);

    uart_event_t event;
    if (xQueueReceive(uart0_queue, &event, timeout))
    {
        if (event.type == UART_DATA)
        {
            // parse the JSON payload
            char *input = malloc(event.size + 1);
            input[event.size] = 0; // null termination
            uart_read_bytes(UART_NUM_0, (uint8_t *)input, event.size, portMAX_DELAY);
            cJSON *const root = cJSON_Parse(input);

            // copy the setup values to char pointers
            if (cJSON_HasObjectItem(root, "ssid") && cJSON_HasObjectItem(root, "password"))
            {
                memcpy(wifi_config->sta.ssid, cJSON_GetStringValue(cJSON_GetObjectItem(root, "ssid")), 32);
                memcpy(wifi_config->sta.password, cJSON_GetStringValue(cJSON_GetObjectItem(root, "password")), 64);
                if (strlen((char *)wifi_config->sta.password) == 0)
                {
                    // TODO: if no password, set authmode for unsecure connection
                }
                wifi_config->sta.pmf_cfg.capable = true;
            }

            // free memory
            cJSON_Delete(root);
            free(input);

            // checky data validity
            if (strlen((char *)wifi_config->sta.ssid))
            {
                puts(JSON_OK);
                ESP_LOGD(TAG, "received ssid: %s", wifi_config->sta.ssid);
                ret = ESP_OK;

                // register the wifi response handler
                connect_attempts = 0;
                esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID,
                                           response_handler, NULL);
            }
            else
            {
                puts(JSON_BAD_PAYLOAD);
                ESP_LOGW(TAG, "unable to parse payload");
                ret = ESP_ERR_INVALID_ARG;
            }
        }
        else
        {
            // an unexpected uart event occurred
            printf(JSON_UNEXPECTED_EVENT "%d", event.type);
            ESP_LOGW(TAG, "unexpected uart event type: %d", event.type);
            uart_flush_input(UART_NUM_0);
            ret = ESP_ERR_INVALID_RESPONSE;
        }
    }
    else
    {
        // the payload wasn't received
        ret = ESP_ERR_TIMEOUT;
    }

    return ret;
}
