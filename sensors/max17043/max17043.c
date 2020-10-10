#include "max17043.h"

#include <math.h>
#include "i2c.h"

/** Datasheet can be found here:
 *   http://cdn.sparkfun.com/datasheets/Prototyping/MAX17043-MAX17044.pdf
*/

#define DEVICE_ADDRESS 0x36
#define VCELL_REG 0x02
#define SOC_REG 0x04
#define MODE_REG 0x06
#define VERSION_REG 0x08
#define CONFIG_REG 0x0c
#define COMMAND_REG 0xfe

#define DEFAULT_WAIT_TIME 100 / portTICK_PERIOD_MS

esp_err_t max17043_reset()
{
    const uint8_t reset_word[2] = {0x54, 0x00}; // power-on reset command
    return i2c_bus_write(DEVICE_ADDRESS, COMMAND_REG, reset_word, 2, DEFAULT_WAIT_TIME);
}

esp_err_t max17043_set_config(const max17043_config_t *config)
{
    return i2c_bus_write(DEVICE_ADDRESS, CONFIG_REG, config, 2, DEFAULT_WAIT_TIME);
}

esp_err_t max17043_get_config(max17043_config_t *config)
{
    uint8_t buf[2];
    esp_err_t err = i2c_bus_read(DEVICE_ADDRESS, CONFIG_REG, buf, 2, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    config->config.val = buf[0] << 8 | buf[1];

    err = i2c_bus_read(DEVICE_ADDRESS, MODE_REG, buf, 2, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    config->mode = buf[0] << 8 | buf[1];
    return ESP_OK;
}

esp_err_t max17043_get_data(max17043_data_t *data)
{
    uint8_t buf[4];
    esp_err_t err = i2c_bus_read(DEVICE_ADDRESS, VCELL_REG, buf, 4, DEFAULT_WAIT_TIME);
    if (err)
        return err;
        
    data->millivolts = ((buf[0] << 8 | buf[1]) >> 4) * 1.25;
    data->battery_life = (buf[2] << 8 | buf[3]) / 256.0;

    return ESP_OK;
}

uint8_t max17043_alert_threshold(uint8_t percentage)
{
    return 32 - percentage;
}

esp_err_t max17043_get_version(uint16_t *version)
{
    uint8_t buf[2];
    esp_err_t err = i2c_bus_read(DEVICE_ADDRESS, VERSION_REG, buf, 2, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    *version = buf[0] << 8 | buf[1];
    return ESP_OK;
}