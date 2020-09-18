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
    max17043_start();
    pms5003_start();

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

    pms5003_set_power(1);

    esp_err_t bme_err = bme280_reset();
    if (bme_err)
        ESP_LOGE(TAG, "BME soft reset error %x", bme_err);
    bme280_config_t weather_monitoring_config = {
        .config = {
            .spi3w_en = 0, 
            .t_sb = 0, 
            .filter = 0
        }, 
        .ctrl_meas = {
            .mode = 1, 
            .osrs_p = 1, 
            .osrs_t = 1
        }, 
        .ctrl_hum = {
            .osrs_h = 1
        }
    };
    bme_err = bme280_set_config(&weather_monitoring_config);
    if (bme_err)
        ESP_LOGE(TAG, "BME set config error %x", bme_err);

    bme280_get_config(&weather_monitoring_config);
    printf("received config: %02x, ctrl_meas: %02x, ctrl_hum: %02x\n",
           weather_monitoring_config.config.val, weather_monitoring_config.ctrl_meas.val,
           weather_monitoring_config.ctrl_hum.val);

    while (1)
    {
        bme280_data_t bme_data;
        bme280_force_measurement();
        bme280_get_data(&bme_data);
        printf("It's %.2f C, with %.2f%%RH, and %lld Pa pressure\n", bme_data.temperature,
               bme_data.humidity, bme_data.pressure);

        pms5003_data_t pms_data;
        esp_err_t pms_err = pms5003_get_data(&pms_data);
        printf("PM2.5 at %d (checksum ", pms_data.concAtm.pm2_5);
        if (pms_err == ESP_OK)
            printf("OK)\n");
        else
            printf("FAIL)\n");

        printf("Battery at %.2f%% (%.1fmV)\n",
               max17043_get_battery_percentage(),
               max17043_get_battery_millivolts());

        gpio_set_level(GPIO_NUM_13, !gpio_get_level(GPIO_NUM_13));

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
