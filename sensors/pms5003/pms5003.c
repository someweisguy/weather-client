#include "pms5003.h"

#include <string.h>
#include "driver/gpio.h"
#include "uart.h"

#define PIN_NUM_SET 21
#define PIN_NUM_RST 19

#define DEFAULT_WAIT_TIME 100 / portTICK_PERIOD_MS

static pms5003_config_t device_config;
static int64_t fan_on_tick = -1;

esp_err_t pms5003_reset()
{
    // reset the gpio
    gpio_reset_pin(PIN_NUM_SET);
    gpio_set_direction(PIN_NUM_SET, GPIO_MODE_INPUT_OUTPUT);
    gpio_reset_pin(PIN_NUM_RST);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_INPUT_OUTPUT);

    // reset the pms5003
    gpio_set_level(PIN_NUM_RST, 0);
    gpio_set_level(PIN_NUM_RST, 1);

    // set the default config
    device_config.mode = PMS5003_ACTIVE;
    device_config.sleep = PMS5003_WAKEUP;

    return ESP_OK;
}

esp_err_t pms5003_get_config(pms5003_config_t *config)
{
    memcpy(config, &device_config, sizeof(device_config));
    return ESP_OK;
}

esp_err_t pms5003_set_config(const pms5003_config_t *config)
{
    if (config->mode > 1 || config->sleep > 1)
        return ESP_ERR_INVALID_ARG;

    // set the device mode
    if (config->mode != device_config.mode)
    {
        uint8_t cmd[] = {0x42, 0x4d, 0xe1, 0x00, 0x00, 0x01, 0x70};
        cmd[4] += config->mode;
        cmd[6] += config->mode;
        esp_err_t err = uart_bus_write(cmd, sizeof(cmd), DEFAULT_WAIT_TIME);
        if (err)
            return err;
        device_config.mode = config->mode;
    }

    // set the device sleep
    if (config->sleep != device_config.sleep)
    {
        uint8_t cmd[] = {0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73};
        cmd[4] += config->sleep;
        cmd[6] += config->sleep;
        esp_err_t err = uart_bus_write(cmd, sizeof(cmd), DEFAULT_WAIT_TIME);
        if (err)
            return err;
        device_config.sleep = config->sleep;
    }
    return ESP_OK;
}

esp_err_t pms5003_get_data(pms5003_data_t *data)
{
    data->checksum_ok = false; // assume data is bad
    if (fan_on_tick >= 0)
        data->fan_on_time = (esp_timer_get_time() - fan_on_tick) / 1000;
    else
        data->fan_on_time = -1;

    // allocate a buffer and read the data in
    uint8_t buffer[32];
    if (device_config.mode == PMS5003_PASSIVE)
    {
        // request data from the device
        const uint8_t cmd[] = {0x42, 0x4d, 0xe2, 0x00, 0x00, 0x01, 0x71};
        esp_err_t err = uart_bus_write(cmd, sizeof(cmd), DEFAULT_WAIT_TIME);
        if (err)
            return err;
    }

    // read the data from the device
    esp_err_t err = uart_bus_read(buffer, 32, DEFAULT_WAIT_TIME);
    if (err)
        return err;

    // copy data over, swap endianness
    for (int i = 4; i < 28; i += 2)
    {
        ((uint8_t *)data)[i - 4] = buffer[i + 1];
        ((uint8_t *)data)[i - 3] = buffer[i];
    }

    // validate checksum
    uint16_t checksum = 0xab; // first 4 bytes always sum to 0xab
    for (int i = 4; i < 30; ++i)
        checksum += buffer[i];
    if (checksum == (buffer[30] << 8 | buffer[31]))
        data->checksum_ok = true;

    return ESP_OK;
}

esp_err_t pms5003_set_power(uint32_t level)
{
    esp_err_t err = gpio_set_level(PIN_NUM_SET, level);
    if (!err)
    {
        if (level == 1)
            fan_on_tick = esp_timer_get_time();
        else
            fan_on_tick = -1;
    }
    return err;
}
