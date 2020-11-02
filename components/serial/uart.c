#include "uart.h"

#include "driver/uart.h"

#define CONFIG_UART_PORT 1  // default UART port
#define PIN_NUM_TX 17       // Adafruit Feather 32 Default
#define PIN_NUM_RX 16       // Adafruit Feather 32 Default

static bool started = false;

esp_err_t uart_init() {
  if (started) return ESP_OK;

  // configure the uart port for the pms5003
  const uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
  };
  uart_param_config(CONFIG_UART_PORT, &uart_config);
  uart_set_pin(CONFIG_UART_PORT, PIN_NUM_TX, PIN_NUM_RX, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
  esp_err_t err = uart_driver_install(CONFIG_UART_PORT, 255, 0, 10, NULL, 0);
  if (!err) started = true;
  return err;
}

esp_err_t uart_deinit() {
  uart_flush(CONFIG_UART_PORT);
  return uart_driver_delete(CONFIG_UART_PORT);
}

esp_err_t uart_bus_write(const void *buf, size_t size, TickType_t timeout) {
  if (size == 0) return ESP_OK;

  const int written = uart_write_bytes(CONFIG_UART_PORT, buf, size);

  esp_err_t err = uart_wait_tx_done(CONFIG_UART_PORT, timeout);
  if (written != size)
    return ESP_ERR_TIMEOUT;
  else
    return err;
}

esp_err_t uart_bus_read(void *buf, size_t size, TickType_t timeout) {
  if (size == 0) return ESP_OK;

  // only read new data
  uart_flush_input(CONFIG_UART_PORT);

  const int read = uart_read_bytes(CONFIG_UART_PORT, buf, size, timeout);

  if (read != size)
    return ESP_ERR_TIMEOUT;
  else
    return ESP_OK;
}
