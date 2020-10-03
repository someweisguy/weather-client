#include <sys/time.h>

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

static const char *TAG = "main";

uint64_t up_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

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
    const pms5003_config_t pms_config = PMS5003_PASSIVE_AWAKE;
    pms5003_set_config(&pms_config);
    
    max17043_reset();
    const max17043_config_t max_config = MAX17043_DEFAULT_CONFIG;
    max17043_set_config(&max_config);
    
    bme280_reset();
    const bme280_config_t bme_config = BME280_WEATHER_MONITORING;
    bme280_set_config(&bme_config);

    sph0645_reset();
    const sph0645_config_t sph_config = SPH0645_DEFAULT_CONFIG;
    sph0645_set_config(&sph_config);

    // init non-volatile storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "erasing nvs flash");
        nvs_flash_erase();
        if (nvs_flash_init() != ESP_OK)
            esp_restart();
    }

    wlan_start();

    while (1)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        printf("System uptime is %lld ms\n", up_time());
        
        bme280_data_t bme_data;
        bme280_force_measurement();
        bme280_get_data(&bme_data);
        printf("It's %.2f F, with %.2f%%RH, the dew point is %.2f F, and the pressure is %.1f Pa\n", 
            bme_data.temperature * 9.0/5.0 + 32, bme_data.humidity, 
            bme_data.dew_point * 9.0/5.0 + 32, bme_data.pressure);

        pms5003_data_t pms_data;
        pms5003_get_data(&pms_data);
        printf("PM2.5 at %d (checksum %s, fan on for %lld ms)\n", pms_data.concAtm.pm2_5, 
            pms_data.checksum_ok ? "OK" : "FAIL", pms_data.fan_on_time);

        sph0645_data_t sph_data;
        sph0645_get_data(&sph_data);
        printf("Min: %.3f dBC, Max: %.3f dBC, Avg: %.3f dBC (%lld samples)\n",
               sph_data.min, sph_data.max, sph_data.avg, sph_data.samples);

        wlan_data_t wlan_data;
        wlan_get_data(&wlan_data);

        max17043_data_t max_data;
        max17043_get_data(&max_data);
        printf("Battery at %.2f%% (%.1fmV), RSSI is at %i\n", max_data.battery_life, 
            max_data.millivolts, wlan_data.rssi);
        
        puts("\n");

    }
}
