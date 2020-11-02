#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sensor_mgmt.h"
#include "wireless.h"
#include <sys/time.h>

static const char *TAG = "main";

void timer_callback(void *args);

void app_main(void)
{
    esp_event_loop_create_default();

    // init non-volatile storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "erasing nvs flash");
        nvs_flash_erase();
        if (nvs_flash_init() != ESP_OK)
            esp_restart();
    }

    // start wireless (blocks) and sensors
    wireless_start(CONFIG_MQTT_BROKER_URI);
    sensors_start(NULL);

    // configure and create a periodic timer
    esp_timer_init();
    const esp_timer_create_args_t timer_args = {
        .callback = timer_callback};
    esp_timer_handle_t main_timer;
    esp_timer_create(&timer_args, &main_timer);

    // wait for the wakeup time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    const time_t ms_to_wakeup = (300 - tv.tv_sec % 300 - 1 - 32) * 1000 + (1000 - (tv.tv_usec / 1000));
    vTaskDelay(ms_to_wakeup / portTICK_PERIOD_MS);

    // start the periodic timer and call the callback
    esp_timer_start_periodic(main_timer, 5 * 60 * 1000 * 1000); // 5 minutes
    timer_callback(NULL);
}

void timer_callback(void *args)
{
    TickType_t wake_tick = xTaskGetTickCount();

    // wake up sensors and report results
    cJSON *json = cJSON_CreateObject();
    sensors_wakeup(json);
    mqtt_publish_json("test", json, 2, false);
    cJSON_Delete(json);
    ESP_LOGI(TAG, "Woke up!");

    // wait 32 seconds
    vTaskDelayUntil(&wake_tick, (32 * 1000) / portTICK_PERIOD_MS);

    // get data and report results
    json = cJSON_CreateObject();
    sensors_get_data(json);
    mqtt_publish_json("test", json, 2, false);
    cJSON_Delete(json);
    ESP_LOGI(TAG, "Took data!");

    // sleep sensors and report results
    json = cJSON_CreateObject();
    sensors_sleep(json);
    mqtt_publish_json("test", json, 2, false);
    cJSON_Delete(json);
    ESP_LOGI(TAG, "Went to sleep!");
}