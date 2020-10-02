#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#include "i2c.h"
#include "uart.h"
#include "i2s.h"

#include "wlan.h"

#include "driver/gpio.h"

static const char *TAG = "main";

void app_main(void)
{
    // create default event loop
    esp_event_loop_create_default();

    // start serial communications
    i2c_start();
    uart_start();
    i2s_init();

    // init sensors
    pms5003_reset();
    
    max17043_reset();
    
    bme280_reset();
    const bme280_config_t bme_config = BME280_WEATHER_MONITORING;
    bme280_set_config(&bme_config);

    sph0645_reset();
    const sph0645_config_t sph_config = SPH0645_DEFAULT_CONFIG;
    if (sph0645_set_config(&sph_config) != ESP_OK)
        printf("ERROR!!!!\n");

    // init non-volatile storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "erasing nvs flash");
        nvs_flash_erase();
        if (nvs_flash_init() != ESP_OK)
            esp_restart();
    }

    wifi_start();

    sph0645_data_t data;

    while (1)
    {
        /*
        bme280_data_t bme_data;
        bme280_force_measurement();
        bme280_get_data(&bme_data);
        printf("It's %.2f F, with %.2f%%RH, and %lld Pa pressure\n", bme_data.temperature * 9.0/5.0 + 32,
               bme_data.humidity, bme_data.pressure);

        pms5003_data_t pms_data;
        pms5003_get_data(&pms_data);
        printf("PM2.5 at %d (checksum %s)\n", pms_data.concAtm.pm2_5, 
            pms_data.checksum_ok ? "OK" : "FAIL");

        max17043_data_t max_data;
        max17043_get_data(&max_data);
        printf("Battery at %.2f%% (%.1fmV)\n", max_data.battery_life, 
            max_data.millivolts);
        */

        sph0645_get_data(&data);
        printf("Min: %.3f dBC, Max: %.3f dBC, Avg: %.3f dBC (%lld samples)\n",
               data.min, data.max, data.avg, data.samples);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
