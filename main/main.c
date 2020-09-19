#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"

#include "i2c.h"
#include "uart.h"
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

    // init sensors
    
    

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

    gpio_reset_pin(GPIO_NUM_13);
    gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT_OUTPUT);

    pms5003_reset();
    pms5003_set_power(1);

    max17043_reset();
    max17043_config_t max_config;
    max17043_get_config(&max_config);
    printf("max17043 config: %04x, mode: %04x\n", max_config.config.val, 
        max_config.mode);
    
    bme280_reset();
    bme280_config_t bme_config = BME280_WEATHER_MONITORING;
    bme280_set_config(&bme_config);
  

    while (1)
    {
        bme280_data_t bme_data;
        bme280_force_measurement();
        bme280_get_data(&bme_data);
        printf("It's %.2f F, with %.2f%%RH, and %lld Pa pressure\n", bme_data.temperature * 9.0/5.0 + 32,
               bme_data.humidity, bme_data.pressure);

        pms5003_data_t pms_data;
        esp_err_t pms_err = pms5003_get_data(&pms_data);
        printf("PM2.5 at %d (checksum ", pms_data.concAtm.pm2_5);
        if (pms_data.checksum_ok)
            printf("OK)\n");
        else
            printf("FAIL)\n");

        max17043_data_t max_data;
        max17043_get_data(&max_data);
        printf("Battery at %.2f%% (%.1fmV)\n", max_data.battery_life, 
            max_data.millivolts);

        gpio_set_level(GPIO_NUM_13, !gpio_get_level(GPIO_NUM_13));

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
