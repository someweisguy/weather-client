#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <sys/time.h>

#include "wireless.h"

#include "bme280.h"
#include "max17043.h"
#include "pms5003.h"
#include "sph0645.h"

static const char *TAG = "main";

void timer_callback(void *args);

void app_main(void)
{
    // create default event loop
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

    wireless_start("mqtt://192.168.0.2");
    // TODO: send discovery strings

    // init sensors
    pms5003_reset();
    const pms5003_config_t pms_config = PMS5003_PASSIVE_ASLEEP;
    pms5003_set_config(&pms_config);

    bme280_reset();
    const bme280_config_t bme_config = BME280_WEATHER_MONITORING;
    bme280_set_config(&bme_config);
    const double elevation = wireless_get_elevation();
    bme280_set_elevation(elevation);

    sph0645_reset();
    const sph0645_config_t sph_config = SPH0645_DEFAULT_CONFIG;
    sph0645_set_config(&sph_config);


    // configure a periodic timer for every 5 minutes
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
    esp_timer_start_periodic(main_timer, 5 * 60 * 1000 * 1000);
    timer_callback(NULL);
}

void timer_callback(void *args)
{
    // wake up sensors
    TickType_t wake_tick = xTaskGetTickCount();
    ESP_LOGI(TAG, "Woke up!");
    mqtt_publish("test", "Woke up!", 2, false);

    // wait 32 seconds
    vTaskDelayUntil(&wake_tick, (32 * 1000) / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Took data!");
    mqtt_publish("test", "Took data!", 2, false);
}