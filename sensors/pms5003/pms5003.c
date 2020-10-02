#include "pms5003.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "uart.h"

#define PIN_NUM_SET 21
#define PIN_NUM_RST 19

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

    // turn off the pms5003
    gpio_set_level(PIN_NUM_SET, 0);
    gpio_set_level(PIN_NUM_RST, 1);

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

esp_err_t pms5003_get_data(pms5003_data_t *data)
{
    data->checksum_ok = false; // assume data is bad
    if (fan_on_tick >= 0)
        data->fan_on_time_ms = (esp_timer_get_time() / fan_on_tick) / 1000;
    else
        data->fan_on_time_ms = -1;

    // allocate a buffer and read the data in
    uint8_t buffer[32];
    uart_flush_input(CONFIG_UART_PORT);
    esp_err_t err = uart_bus_read(buffer, 32, 500);
    if (err)
        return err;

    // copy data over, swap endianness
    for (int i = 4; i < 28; i += 2)
    {
        ((uint8_t*) data)[i - 4] = buffer[i + 1];
        ((uint8_t*) data)[i - 3] = buffer[i];
    }

    // validate checksum
    uint16_t checksum = 0xab; // first 4 bytes always sum to 0xab
    for (int i = 4; i < 30; ++i)
        checksum += buffer[i];
    if (checksum == (buffer[30] << 8 | buffer[31]))
        data->checksum_ok = true;
    
    return ESP_OK;    
}
