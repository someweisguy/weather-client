#include "i2c.h"
#include "driver/i2c.h"

#define I2C_PORT I2C_NUM_1
#define PIN_NUM_SDA 23 // Adafruit Feather 32 Default
#define PIN_NUM_SCL 22 // Adafruit Feather 32 Default

static esp_err_t i2c_master_command(char addr, char reg, void *buf, size_t size,
									time_t wait_ms, const uint8_t READ_BIT)
{
	if (size == 0)
		return ESP_OK;

	// create the command handle
	const i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	// queue the start, address, and register
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg, true);

	// queue the message payload
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, true);
	if (size > 1)
		i2c_master_read(cmd, buf, size - 1, I2C_MASTER_ACK);
	i2c_master_read_byte(cmd, buf + size - 1, I2C_MASTER_NACK);
	i2c_master_stop(cmd);

	// Get the amount of ticks to wait
	const TickType_t ticks = wait_ms == 0 ? portMAX_DELAY : wait_ms / portTICK_PERIOD_MS;

	esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, ticks);
	i2c_cmd_link_delete(cmd);
	return err;
}

esp_err_t i2c_start()
{
	const i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,		// set to master mode
		.master = {
			.clk_speed = 100000			// standard i2c speed
		},
		.sda_io_num = PIN_NUM_SDA,
		.scl_io_num = PIN_NUM_SCL,
		.sda_pullup_en = false,			// enable built-in pullup
		.scl_pullup_en = false,			// enable built-in pullup
	};
	esp_err_t err = i2c_param_config(I2C_PORT, &i2c_config);
	if (err)
		return err;
	err = i2c_driver_install(I2C_PORT, i2c_config.mode, 0, 0, 0);
	return err;
}

esp_err_t i2c_stop()
{
	return i2c_driver_delete(I2C_PORT);
}

esp_err_t i2c_read(char addr, char reg, void *buf, size_t size, time_t wait_ms)
{
	return i2c_master_command(addr, reg, buf, size, wait_ms, 1);
}

esp_err_t i2c_write(char addr, char reg, void *buf, size_t size, time_t wait_ms)
{
	return i2c_master_command(addr, reg, buf, size, wait_ms, 0);
}
