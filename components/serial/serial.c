#include "serial.h"

#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"

#define I2C_PORT    1
#define PIN_NUM_SDA 23
#define PIN_NUM_SCL 22
#define UART_PORT   1
#define PIN_NUM_TX  17
#define PIN_NUM_RX  16

static const char *TAG = "serial";
static QueueHandle_t uart_queue;

esp_err_t serial_start() {
  // config and init i2c with standard speed and pullups enabled
  const i2c_config_t i2c_config = {
    .mode = I2C_MODE_MASTER,
    .master = {.clk_speed = 100000},
    .sda_io_num = PIN_NUM_SDA,
    .scl_io_num = PIN_NUM_SCL,
    .sda_pullup_en = true,
    .scl_pullup_en = true
  };
  i2c_param_config(I2C_PORT, &i2c_config);
  esp_err_t err = i2c_driver_install(I2C_PORT, i2c_config.mode, 0, 0, 
    ESP_INTR_FLAG_LOWMED);
  if (err) {
    ESP_LOGE(TAG, "An error occurred installing the I2C driver");
    return err;
  }
  
  // config and init uart for plantower pms5003/7003
  uart_set_pin(UART_PORT, PIN_NUM_TX, PIN_NUM_RX, -1, -1);
  const uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  uart_param_config(UART_PORT, &uart_config);
  err = uart_driver_install(UART_PORT, 256, 0, 10, &uart_queue, 
    ESP_INTR_FLAG_LOWMED);
  if (err) {
    ESP_LOGE(TAG, "An error occurred installing the UART driver");
    return err;
  }

  return ESP_OK;
}

static esp_err_t i2c_cmd(char addr, char reg, void *buf, size_t size, 
    bool is_read_cmd, bool ack_en, TickType_t timeout) {
  // create the command handle
  const i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  // write a start signal
  i2c_master_start(cmd);

  // write the i2c address and register to the command handle
  i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, ack_en);
  i2c_master_write_byte(cmd, reg, ack_en);

  // check if this is a read command
  if (is_read_cmd) {
    // read from the i2c client
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (size > 1) i2c_master_read(cmd, buf, size - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, buf + size - 1, I2C_MASTER_NACK);
  } else {
    // write to the i2c client
    i2c_master_write(cmd, buf, size, ack_en);
  }

  // write a stop signal
  i2c_master_stop(cmd);

  // send the i2c command
  esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, timeout);
  if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Timed out waiting for I2C host");
  }
  i2c_cmd_link_delete(cmd);
  return err;
}

esp_err_t serial_i2c_read(char addr, char reg, void *buf, size_t size, 
    TickType_t timeout) {
  return i2c_cmd(addr, reg, buf, size, true, false, timeout);
}

esp_err_t serial_i2c_write(char addr, char reg, void *buf, size_t size, bool ack_en,
    TickType_t timeout) {
  return i2c_cmd(addr, reg, buf, size, false, ack_en, timeout);
}

esp_err_t serial_uart_read(void *buf, size_t size, TickType_t timeout) {
  // wait for data on the event queue
  int rxd_data_size = 0;
  do {
    uart_event_t event;
    const TickType_t start_tick = xTaskGetTickCount();
    if (xQueueReceive(uart_queue, &event, timeout) == pdFALSE) {
      ESP_LOGE(TAG, "Timed out waiting for UART data");
      return ESP_ERR_TIMEOUT;
    }
    timeout -= xTaskGetTickCount() - start_tick;
    if (event.type == UART_DATA) rxd_data_size += event.size;
  } while (rxd_data_size < size);
  
  // read the data from the uart buffer
  const int read = uart_read_bytes(UART_PORT, buf, size, timeout);
  if (read != size) {
    if (read == -1) ESP_LOGE(TAG, "Timed out reading from the UART buffer");
    else ESP_LOGE(TAG, "An error occurred reading from the UART buffer");
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t serial_uart_write(const void *src, size_t size) {
  // returns after the data has been written because tx buffer len is 0
  return uart_write_bytes(UART_PORT, src, size);
}