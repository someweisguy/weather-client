#include "bme280.h"

#include <math.h>
#include <string.h>
#include "i2c.h"
#include "nvs_flash.h"
#include "nvs.h"

#define I2C_ADDRESS 0x76
#define REG_CHIP_ID 0xd0
#define REG_RESET 0xe0
#define REG_CTRL_HUM 0xf2
#define REG_STATUS 0xf3
#define REG_CTRL_MEAS 0xf4
#define REG_CONFIG 0xf5
#define REG_DATA_START 0xf7
#define REG_TRIM_T1_TO_H1 0x88
#define REG_TRIM_H2_TO_H6 0xe1

#define ELEVATION_NVS_PAGE "bme"
#define ELEVATION_NVS_KEY "elevation"
#define MEASURING_BIT 8
#define IM_UPDATE_BIT 1

#define DEFAULT_WAIT_TIME 100 / portTICK_PERIOD_MS

#define MAX(a, b) (a > b ? a : b)

static int32_t elevation; // The elevation of the weather station (meters). Used to compensate pressure at current elevation from sea level.

static struct
{
    uint16_t t1;
    int16_t t2;
    int16_t t3;

    uint16_t p1;
    int16_t p2;
    int16_t p3;
    int16_t p4;
    int16_t p5;
    int16_t p6;
    int16_t p7;
    int16_t p8;
    int16_t p9;

    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4;
    int16_t h5;
    int8_t h6;
} dig; // Trimming parameters.

static int32_t calculate_t_fine(const int32_t adc_T)
{
    // This mess of code taken straight from the datasheet. Best not to mess with it.
    const int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig.t1 << 1))) * ((int32_t)dig.t2)) >> 11;
    const int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig.t1)) * ((adc_T >> 4) - ((int32_t)dig.t1))) >> 12) * ((int32_t)dig.t3)) >> 14;
    const int32_t T = var1 + var2;
    return T;
}

static int32_t compensate_temperature(const int32_t t_fine)
{
    // Return temperature in 1/100ths of a degree Celsius
    return (t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_pressure(const int32_t t_fine, const int32_t adc_P)
{
    // This mess of code taken straight from the datasheet. Best not to mess with it.
    // Return pressure in Pascals * 256
    int64_t var1, var2, P;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig.p6;
    var2 = var2 + ((var1 * (int64_t)dig.p5) << 17);
    var2 = var2 + (((int64_t)dig.p4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig.p3) >> 8) + ((var1 * (int64_t)dig.p2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig.p1) >> 33;
    if (var1 == 0)
        return 0; // avoid divide by zero
    P = 1048576 - adc_P;
    P = (((P << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig.p9) * (P >> 13) * (P >> 13)) >> 25;
    var2 = (((int64_t)dig.p8) * P) >> 19;
    P = ((P + var1 + var2) >> 8) + (((int64_t)dig.p7) << 4);

    return P;
}

static uint32_t compensate_humidity(const int32_t t_fine, const int32_t adc_H)
{
    // This mess of code taken straight from the datasheet. Best not to mess with it.
    // Return relative humidity * 1024
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig.h4) << 20) - (((int32_t)dig.h5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig.h6)) >> 10) * (((v_x1_u32r * ((int32_t)dig.h3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)dig.h2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig.h1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    const uint32_t H = (uint32_t)(v_x1_u32r >> 12);

    return H;
}

static esp_err_t wait_for_device(uint8_t bit_to_wait_for)
{
    for (uint8_t bit = 1; bit & bit_to_wait_for;)
    {
        // Wait for the device to be ready by reading the status register
        esp_err_t err = i2c_bus_read(I2C_ADDRESS, REG_RESET, &bit, 1, DEFAULT_WAIT_TIME);
        if (err)
            return err;
    }
    return ESP_OK;
}

esp_err_t bme280_reset()
{
    i2c_init();

    const uint8_t soft_reset_word = 0xb6; // The soft reset word which resets the device using the complete power-on-reset procedure.
    esp_err_t err = i2c_bus_write(I2C_ADDRESS, REG_RESET, &soft_reset_word, 1, DEFAULT_WAIT_TIME);
    if (err)
        return err;

    err = wait_for_device(IM_UPDATE_BIT);
    if (err)
        return err;

    // read the trimming parameters and copy them to memory
    char buf[32];
    err = i2c_bus_read(I2C_ADDRESS, REG_TRIM_T1_TO_H1, buf, 25, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    err = i2c_bus_read(I2C_ADDRESS, REG_TRIM_H2_TO_H6, buf + 25, 7, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    memcpy(&dig, buf, 25);
    dig.h2 = buf[26] << 8 | buf[25];
    dig.h3 = buf[27];
    dig.h4 = buf[28] << 4 | (buf[29] & 0x0f);
    dig.h5 = buf[30] << 4 | buf[29] >> 4;
    dig.h6 = buf[31];

    // load elevation from nvs
    nvs_handle_t nvs;
    err = nvs_open(ELEVATION_NVS_PAGE, NVS_READWRITE, &nvs);
    if (err)
        return err;
    err = nvs_get_i32(nvs, ELEVATION_NVS_KEY, &elevation);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        // elevation hasn't been initialized in nvs yet
        elevation = 0; // default is sea level
        err = nvs_set_i32(nvs, ELEVATION_NVS_KEY, elevation);
        if (!err)
            err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    return err;
}

esp_err_t bme280_set_config(const bme280_config_t *config)
{
    // set device to sleep mode or else changes won't take
    const uint8_t sleep_word = 0;
    esp_err_t err = i2c_bus_write(I2C_ADDRESS, REG_CTRL_MEAS, &sleep_word, 1, DEFAULT_WAIT_TIME);
    if (err)
        return err;

    // Writes must be made in this order
    err = i2c_bus_write(I2C_ADDRESS, REG_CONFIG, &(config->config.val), 1, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    err = i2c_bus_write(I2C_ADDRESS, REG_CTRL_HUM, &(config->ctrl_hum.val), 1, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    err = i2c_bus_write(I2C_ADDRESS, REG_CTRL_MEAS, &(config->ctrl_meas.val), 1, DEFAULT_WAIT_TIME);
    return err;
}

esp_err_t bme280_get_config(bme280_config_t *config)
{
    esp_err_t err = i2c_bus_read(I2C_ADDRESS, REG_CONFIG, &(config->config.val), 1, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    err = i2c_bus_read(I2C_ADDRESS, REG_CTRL_MEAS, &(config->ctrl_meas.val), 1, DEFAULT_WAIT_TIME);
    if (err)
        return err;
    err = i2c_bus_read(I2C_ADDRESS, REG_CTRL_HUM, &(config->ctrl_hum.val), 1, DEFAULT_WAIT_TIME);
    return err;
}

esp_err_t bme280_force_measurement()
{
    bme280_config_t config;
    esp_err_t err = i2c_bus_read(I2C_ADDRESS, REG_CTRL_MEAS, &(config.ctrl_meas.val), 1, DEFAULT_WAIT_TIME);
    if (err)
        return err;

    // return error if not in forced or sleep mode
    if (config.ctrl_meas.mode == BME280_NORMAL_MODE)
        return ESP_ERR_INVALID_STATE;

    // set the device to forced measurement mode
    config.ctrl_meas.mode = BME280_FORCED_MODE;
    err = i2c_bus_write(I2C_ADDRESS, REG_CTRL_MEAS, &(config.ctrl_meas.val), 1, DEFAULT_WAIT_TIME);
    return err;
}

esp_err_t bme280_get_data(bme280_data_t *data)
{
    esp_err_t err = wait_for_device(MEASURING_BIT);
    if (err)
        return err;

    // get uncompensated data from the device
    uint8_t buf[8];
    err = i2c_bus_read(I2C_ADDRESS, REG_DATA_START, buf, 8, DEFAULT_WAIT_TIME);
    if (err)
        return err;

    // swap the endianness and align
    int32_t adc_P = buf[0] << 12 | buf[1] << 4 | buf[2] >> 4,
            adc_T = buf[3] << 12 | buf[4] << 4 | buf[5] >> 4,
            adc_H = buf[6] << 8 | buf[7];
    int32_t t_fine; // used to compensate data

    double celsius; // needed for dew point - can't use F or K!

    // get temperature value
    if (adc_T != 0x80000)
    {
        t_fine = calculate_t_fine(adc_T);
        data->temperature = compensate_temperature(t_fine) / 100.0; // default C
        celsius = data->temperature;
#ifdef CONFIG_FAHRENHEIT
        data->temperature = (data->temperature * 9.0 / 5.0) + 32; // convert to F
#elif defined(CONFIG_KELVIN)
        data->temperature += 273.15; // convert to K
#endif
    }
    else
    {
        // temperature sampling must be turned on to get valid data
        data->temperature = NAN;
        data->humidity = NAN;
        data->pressure = NAN;
        return ESP_ERR_INVALID_STATE;
    }

    // get pressure value
    if (adc_P != 0x80000)
    {
        // compensate for pressure at current_elevation
        const uint32_t pressure_sea_level = compensate_pressure(t_fine, adc_P) / 256;
        const double M = 0.02897,                                                 // molar mass of Eath's air (kg/mol)
            g = 9.807665,                                                         // gravitational constant (m/s^2)
            R = 8.3145,                                                           // universal gas constant (J/mol*K)
            K = data->temperature + 273.15;                                       // temperature in Kelvin
        data->pressure = pressure_sea_level * exp((M * g) / (R * K) * elevation); // default Pa
#ifdef CONFIG_IN_HG
        data->pressure /= 3386.0; // convert to inHg
#elif defined(CONFIG_MM_HG)
        data->pressure /= 133.0;     // convert to mmHg
#endif
    }
    else
        data->pressure = NAN;

    // get humidity value
    if (adc_H != 0x800)
        data->humidity = compensate_humidity(t_fine, adc_H) / 1024.0;
    else
        data->humidity = NAN;

    // calculate the dew point
    if (adc_T != 0x80000 && adc_H != 0x800)
    {
        const double gamma = log(MAX(data->humidity, 0.001) / 100) + ((17.62 * celsius) / (243.12 + celsius));
        data->dew_point = (243.12 * gamma) / (17.32 - gamma); // default C
#ifdef CONFIG_FAHRENHEIT
        data->dew_point = (data->dew_point * 9.0 / 5.0) + 32; // convert to F
#elif defined(CONFIG_KELVIN)
        data->dew_point += 273.15;   // convert to K
#endif
    }
    else
        data->dew_point = NAN;

    return ESP_OK;
}

esp_err_t bme280_get_chip_id(uint8_t *chip_id)
{
    return i2c_bus_write(I2C_ADDRESS, REG_CHIP_ID, chip_id, 1, DEFAULT_WAIT_TIME);
}

int32_t bme280_get_elevation()
{
    return elevation;
}

esp_err_t bme280_set_elevation(int32_t meters)
{
    // avoid unecessary nvs read/write
    if (meters == elevation)
        return ESP_OK;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ELEVATION_NVS_PAGE, NVS_READWRITE, &nvs);
    if (err)
        return err;
    err = nvs_set_i32(nvs, ELEVATION_NVS_KEY, meters);
    if (!err)
        err = nvs_commit(nvs);

    nvs_close(nvs);

    elevation = meters;

    return err;
}