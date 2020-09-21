#include "i2s.h"

#include "esp_log.h"
#include "driver/i2s.h"

#define I2S_NUM 0
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
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };
    esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (err)   
        ESP_LOGE("i2s", "i2s driver install error %x", err);

    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BIT_CLOCK_PIN_NUM,
        .ws_io_num = I2S_WORD_SELECT_PIN_NUM,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_IN_PIN_NUM
    };

    err = i2s_set_pin(I2S_NUM, &pin_config);
    if (err)   
        ESP_LOGE("i2s", "i2s set pin error %x", err);

    return ESP_OK;
}

esp_err_t i2s_end(void)
{
    return i2s_driver_uninstall(I2S_NUM);
}

esp_err_t i2s_master_read(void *buf, size_t size, time_t wait_ms)
{
    size_t bytes_read;
    const TickType_t ticks = wait_ms == 0 ? wait_ms / portTICK_PERIOD_MS : portMAX_DELAY;
    esp_err_t err = i2s_read(I2S_NUM, buf, size, &bytes_read, ticks);
    if (bytes_read != size)
        return ESP_ERR_INVALID_SIZE;
    else
        return err;
}
