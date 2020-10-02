#include "i2s.h"

#include "esp_log.h"
#include "driver/i2s.h"

#define I2S_BIT_CLOCK_PIN_NUM 12
#define I2S_DATA_IN_PIN_NUM 27
#define I2S_WORD_SELECT_PIN_NUM 33

esp_err_t i2s_init(void)
{
    // install i2s driver
    const i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };
    esp_err_t err = i2s_driver_install(CONFIG_I2S_PORT, &i2s_config, 0, NULL);
    if (err)   
        ESP_LOGE("i2s", "i2s driver install error %x", err);

    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BIT_CLOCK_PIN_NUM,
        .ws_io_num = I2S_WORD_SELECT_PIN_NUM,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_IN_PIN_NUM
    };

    err = i2s_set_pin(CONFIG_I2S_PORT, &pin_config);
    if (err)   
        ESP_LOGE("i2s", "i2s set pin error %x", err);

    return ESP_OK;
}

esp_err_t i2s_deinit(void)
{
    return i2s_driver_uninstall(CONFIG_I2S_PORT);
}

esp_err_t i2s_bus_read(void *buf, size_t size, TickType_t timeout)
{
    size_t bytes_read;
	i2s_read(CONFIG_I2S_PORT, buf, size, &bytes_read, timeout);
    if (size != bytes_read)
        return ESP_ERR_TIMEOUT;
    return ESP_OK;
}
