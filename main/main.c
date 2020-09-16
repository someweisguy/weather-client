#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "wlan.h"
#include "i2c.h"

#include "driver/gpio.h"
#include <math.h>

static const char *TAG = "main";

void app_main(void)
{
    // create default event loop
    esp_event_loop_create_default();

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

    i2c_start();

    // reset the fuel gauge
    uint8_t por[2] = {0x54, 0x00};
    i2c_write(0x36, 0xFE, por, 2, 1000);

    gpio_reset_pin(GPIO_NUM_13);
    gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT_OUTPUT);

    while (1)
    {
        uint8_t soc[2] = {};
        uint8_t vcell[2] = {};
        i2c_read(0x36, 0x04, soc, 2, 1000);
        i2c_read(0x36, 0x02, &vcell, 2, 1000);

        double mV = ((vcell[0] << 8 | vcell[1]) >> 4) * 1.25;
        float percent = (soc[0] << 8 | soc[1]) / 256.0;

        printf("Battery at %.2f%% (%.1fmV)\n", percent, mV);

        printf("%f\n", NAN);

        gpio_set_level(GPIO_NUM_13, !gpio_get_level(GPIO_NUM_13));

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
