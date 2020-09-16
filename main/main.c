#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"



#include "max17043.h"

#include "i2c.h"
#include "wlan.h"


#include "driver/gpio.h"


static const char *TAG = "main";

void app_main(void)
{
    
    // create default event loop
    esp_event_loop_create_default();

    // start serial communications
    i2c_start();

    // init sensors
    max17043_start();

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

    while (1)
    {
        printf("Battery at %.2f%% (%.1fmV)\n", 
            max17043_get_battery_percentage(), 
            max17043_get_battery_millivolts());

        gpio_set_level(GPIO_NUM_13, !gpio_get_level(GPIO_NUM_13));

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    
}
