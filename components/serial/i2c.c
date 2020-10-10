#include "i2c.h"
#include "driver/i2c.h"

#define WRITE 0
#define READ 1
#define WRITE_NO_ACK 2

#define PIN_NUM_SDA 23 // Adafruit Feather 32 Default
#define PIN_NUM_SCL 22 // Adafruit Feather 32 Default

static esp_err_t i2c_master_command(char addr, char reg, void *buf, size_t size,
									TickType_t timeout, const uint8_t READ_BIT)
{
	if (size == 0)
		return ESP_OK;

	// create the command handle
	const i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	const bool check_ack = (READ_BIT != WRITE_NO_ACK);

	// queue the start, address, and register
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, check_ack);
	i2c_master_write_byte(cmd, reg, check_ack);

	if (READ_BIT == READ)
	{
		// read from the i2c slave
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (addr << 1) | READ_BIT, true);
		if (size > 1)
			i2c_master_read(cmd, buf, size - 1, I2C_MASTER_ACK);
		i2c_master_read_byte(cmd, buf + size - 1, I2C_MASTER_NACK);
	}
	else
	{
		// write to the i2c slave
		i2c_master_write(cmd, buf, size, check_ack);
	}

	i2c_master_stop(cmd);

	esp_err_t err = i2c_master_cmd_begin(CONFIG_I2C_PORT, cmd, timeout);
	i2c_cmd_link_delete(cmd);
	return err;
}

esp_err_t i2c_start()
{
	const i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER, // set to master mode
		.master = {
			.clk_speed = 100000 // standard i2c speed
		},
		.sda_io_num = PIN_NUM_SDA,
		.scl_io_num = PIN_NUM_SCL,
		.sda_pullup_en = true, // enable built-in pullup
		.scl_pullup_en = true, // enable built-in pullup
	};
	esp_err_t err = i2c_param_config(CONFIG_I2C_PORT, &i2c_config);
	if (err)
		return err;
	err = i2c_driver_install(CONFIG_I2C_PORT, i2c_config.mode, 0, 0, 0);
	return err;
}

esp_err_t i2c_stop()
{
	return i2c_driver_delete(CONFIG_I2C_PORT);
}

esp_err_t i2c_bus_read(char addr, char reg, void *buf, size_t size, TickType_t timeout)
{
	return i2c_master_command(addr, reg, buf, size, timeout, READ);
}

esp_err_t i2c_bus_write(char addr, char reg, const void *buf, size_t size, TickType_t timeout)
{
	return i2c_master_command(addr, reg, (void *)buf, size, timeout, WRITE);
}

esp_err_t i2c_bus_write_no_ack(char addr, char reg, const void *buf, size_t size, TickType_t timeout)
{
	return i2c_master_command(addr, reg, (void *)buf, size, timeout, WRITE_NO_ACK);
}