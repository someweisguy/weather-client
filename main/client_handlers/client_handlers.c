#include "client_handlers.h"

#include <string.h>
#include <math.h>
#include "cJSON.h"

#include "wlan.h"
#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#define TRUNC2(n) (int)(n * 100) / 100.0

static void restart_task(void *args)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

char *data_handler(const bool clear_data)
{
    esp_err_t err;

    // create the main json root
    cJSON *root = cJSON_CreateObject();
    cJSON *system_root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, JSON_ROOT_SYSTEM, system_root);

    // get the wlan data
    wlan_data_t wlan_data;
    err = wlan_get_data(&wlan_data);
    if (!err)
    {
        cJSON_AddNumberToObject(system_root, SYSTEM_WIFI_UP_TIME_KEY, wlan_data.up_time);
        cJSON_AddStringToObject(system_root, SYSTEM_IP_KEY, wlan_data.ip_str);
        cJSON_AddNumberToObject(system_root, SYSTEM_WIFI_RSSI_KEY, wlan_data.rssi);
    }

#ifdef USE_MAX17043
    // get the battery data
    max17043_data_t max_data;
    err = max17043_get_data(&max_data);
    if (!err)
    {
        cJSON_AddNumberToObject(system_root, SYSTEM_BATT_LIFE_KEY, max_data.battery_life);
        cJSON_AddNumberToObject(system_root, SYSTEM_BATT_VOLTAGE_KEY, max_data.millivolts);
    }
#endif // USE_MAX17043

#ifdef USE_BME280
    cJSON *bme_root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, JSON_ROOT_BME, bme_root);

    // get the bme280 data
    bme280_data_t bme_data;
    bme280_force_measurement();
    err = bme280_get_data(&bme_data);
    if (!err)
    {
        cJSON *bme_data_root = cJSON_CreateObject();
        cJSON_AddItemToObject(bme_root, JSON_DATA_KEY, bme_data_root);

        cJSON_AddNumberToObject(bme_data_root, BME_TEMPERATURE_KEY, bme_data.temperature);
        cJSON_AddNumberToObject(bme_data_root, BME_HUMIDITY_KEY, bme_data.humidity);
        cJSON_AddNumberToObject(bme_data_root, BME_PRESSURE_KEY, bme_data.pressure);
        cJSON_AddNumberToObject(bme_data_root, BME_DEW_POINT_KEY, bme_data.dew_point);
    }

    // get the bme280 config
    bme280_config_t bme_config;
    err = bme280_get_config(&bme_config);
    if (!err)
    {
        cJSON *bme_config_root = cJSON_CreateObject(), *bme_config_oversampling;
        cJSON_AddItemToObject(bme_root, JSON_CONFIG_KEY, bme_config_root);

        cJSON_AddItemToObject(bme_config_root, BME_OVERSAMPLING_KEY, bme_config_oversampling = cJSON_CreateObject());
        cJSON_AddNumberToObject(bme_config_oversampling, BME_TEMPERATURE_KEY, bme_config.ctrl_meas.osrs_t);
        cJSON_AddNumberToObject(bme_config_oversampling, BME_HUMIDITY_KEY, bme_config.ctrl_hum.osrs_h);
        cJSON_AddNumberToObject(bme_config_oversampling, BME_PRESSURE_KEY, bme_config.ctrl_meas.osrs_p);
        cJSON_AddNumberToObject(bme_config_root, BME_MODE_KEY, bme_config.ctrl_meas.mode);
        cJSON_AddNumberToObject(bme_config_root, BME_STANDBY_TIME_KEY, bme_config.config.t_sb);
        cJSON_AddNumberToObject(bme_config_root, BME_FILTER_KEY, bme_config.config.filter);
    }

    // create the bme280 elevation node
    const int32_t bme_elevation = bme280_get_elevation();
    cJSON_AddNumberToObject(bme_root, BME_ELEVATION_KEY, bme_elevation);
#endif // USE_BME280

#ifdef USE_PMS5003
    cJSON *pms_root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, JSON_ROOT_PMS, pms_root);

    // get the pms5003 data
    pms5003_data_t pms_data;
    err = pms5003_get_data(&pms_data);
    if (!err)
    {
        cJSON *pms_data_root = cJSON_CreateObject();
        cJSON_AddItemToObject(pms_root, JSON_DATA_KEY, pms_data_root);

        cJSON_AddNumberToObject(pms_data_root, PMS_FAN_UP_TIME_KEY, pms_data.fan_on_time);
        cJSON_AddBoolToObject(pms_data_root, PMS_CHECKSUM_OK_KEY, pms_data.checksum_ok);
        cJSON *pms_standard_node = cJSON_CreateObject();
        cJSON_AddItemToObject(pms_data_root, PMS_STANDARD_PARTICLE_KEY, pms_standard_node);
        cJSON_AddNumberToObject(pms_standard_node, PMS_PM1_KEY, pms_data.concCF1.pm1);
        cJSON_AddNumberToObject(pms_standard_node, PMS_PM2_5_KEY, pms_data.concCF1.pm2_5);
        cJSON_AddNumberToObject(pms_standard_node, PMS_PM10_KEY, pms_data.concCF1.pm10);
        cJSON *pms_atmospheric_node = cJSON_CreateObject();
        cJSON_AddItemToObject(pms_data_root, PMS_ATMOSPHERIC_PARTICLE_KEY, pms_atmospheric_node);
        cJSON_AddNumberToObject(pms_atmospheric_node, PMS_PM1_KEY, pms_data.concAtm.pm1);
        cJSON_AddNumberToObject(pms_atmospheric_node, PMS_PM2_5_KEY, pms_data.concAtm.pm2_5);
        cJSON_AddNumberToObject(pms_atmospheric_node, PMS_PM10_KEY, pms_data.concAtm.pm10);
        cJSON *pms_count_node = cJSON_CreateObject();
        cJSON_AddItemToObject(pms_data_root, PMS_COUNT_PER_0_1L_KEY, pms_count_node);
        cJSON_AddNumberToObject(pms_count_node, PMS_0_3UM_KEY, pms_data.countPer0_1L.um0_3);
        cJSON_AddNumberToObject(pms_count_node, PMS_0_5UM_KEY, pms_data.countPer0_1L.um0_5);
        cJSON_AddNumberToObject(pms_count_node, PMS_1_0UM_KEY, pms_data.countPer0_1L.um1_0);
        cJSON_AddNumberToObject(pms_count_node, PMS_2_5UM_KEY, pms_data.countPer0_1L.um2_5);
        cJSON_AddNumberToObject(pms_count_node, PMS_5_0UM_KEY, pms_data.countPer0_1L.um5_0);
        cJSON_AddNumberToObject(pms_count_node, PMS_10_0UM_KEY, pms_data.countPer0_1L.um10_0);
    }

    // get the pms5003 config
    pms5003_config_t pms_config;
    err = pms5003_get_config(&pms_config);
    if (!err)
    {
        cJSON *pms_config_root = cJSON_CreateObject();
        cJSON_AddItemToObject(pms_root, JSON_CONFIG_KEY, pms_config_root);

        cJSON_AddNumberToObject(pms_config_root, PMS_MODE_KEY, pms_config.mode);
        cJSON_AddNumberToObject(pms_config_root, PMS_SLEEP_SET_KEY, pms_config.sleep);
    }
#endif // USE_PMS5003

#ifdef USE_SPH0645
    cJSON *sph_root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, JSON_ROOT_SPH, sph_root);

    // get the sph0645 data
    sph0645_data_t sph_data;
    err = sph0645_get_data(&sph_data);
    if (!err)
    {
        cJSON *sph_data_root = cJSON_CreateObject();
        cJSON_AddItemToObject(sph_root, JSON_DATA_KEY, sph_data_root);

        cJSON_AddNumberToObject(sph_data_root, SPH_AVG_KEY, sph_data.avg);
        cJSON_AddNumberToObject(sph_data_root, SPH_MIN_KEY, sph_data.min);
        cJSON_AddNumberToObject(sph_data_root, SPH_MAX_KEY, sph_data.max);
        cJSON_AddNumberToObject(sph_data_root, SPH_NUM_SAMPLES_KEY, sph_data.samples);
    }

    // get the sph0645 config
    sph0645_config_t sph_config;
    err = sph0645_get_config(&sph_config);
    if (!err)
    {
        cJSON *sph_config_root = cJSON_CreateObject();
        cJSON_AddItemToObject(sph_root, JSON_CONFIG_KEY, sph_config_root);

        cJSON_AddNumberToObject(sph_config_root, SPH_SAMPLE_LEN_KEY, sph_config.sample_length);
        cJSON_AddNumberToObject(sph_config_root, SPH_SAMPLE_PERIOD_KEY, sph_config.sample_period);
        cJSON_AddNumberToObject(sph_config_root, SPH_SAMPLE_WEIGHTING_KEY, sph_config.weighting);
    }

    // clear or keep the sph0645 data
    if (clear_data)
        sph0645_clear_data();
    cJSON_AddBoolToObject(sph_root, SPH_CLEAR_DATA_KEY, clear_data);
#endif // USE_SPH0645

    // render the json as a string
    char *response = cJSON_Print(root);
    cJSON_Delete(root);

    return response;
}

void restart_handler()
{
    xTaskCreate(restart_task, "restart_task", 2048, NULL, 5, NULL);
}

esp_err_t sensors_set_status(cJSON *root, bool awake)
{
    esp_err_t err;

#ifdef USE_PMS5003
    // get the pms5003 config
    pms5003_config_t pms_config;
    err = pms5003_get_config(&pms_config);
    if (err)
        return err;
    pms_config.sleep = awake;
    err = pms5003_set_config(&pms_config);
    if (err)
        return err;

#endif // USE_PMS5003

    
    // report status back
    const char *status = awake ? MQTT_ON_STATUS : MQTT_OFF_STATUS;
    cJSON_AddStringToObject(root, MQTT_STATUS_KEY, status);

    return ESP_OK;
}

esp_err_t sensors_get_data(cJSON *root)
{
    cJSON *system_root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, JSON_ROOT_SYSTEM, system_root);

    // get the wlan data
    wlan_data_t wlan_data;
    esp_err_t err = wlan_get_data(&wlan_data);
    if (err)
        return err;
    cJSON_AddNumberToObject(system_root, SYSTEM_WIFI_RSSI_KEY, wlan_data.rssi);

#ifdef USE_MAX17043
    // get the battery data
    max17043_data_t max_data;
    err = max17043_get_data(&max_data);
    if (err)
        return err;
    cJSON_AddNumberToObject(system_root, SYSTEM_BATT_LIFE_KEY, (int)max_data.battery_life);
#endif // USE_MAX17043

#ifdef USE_BME280
    // get the bme280 data
    bme280_data_t bme_data;
    bme280_force_measurement();
    err = bme280_get_data(&bme_data);
    if (!err)
    {
        // create the bme280 root
        cJSON *bme_root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, JSON_ROOT_BME, bme_root);

        // add the bme280 to their root
        cJSON_AddNumberToObject(bme_root, BME_TEMPERATURE_KEY, TRUNC2(bme_data.temperature));
        cJSON_AddNumberToObject(bme_root, BME_HUMIDITY_KEY, TRUNC2(bme_data.humidity));
        cJSON_AddNumberToObject(bme_root, BME_PRESSURE_KEY, TRUNC2(bme_data.pressure));
        cJSON_AddNumberToObject(bme_root, BME_DEW_POINT_KEY, TRUNC2(bme_data.dew_point));
    }
#endif // USE_BME280

#ifdef USE_PMS5003
    // get the pms5003 data
    pms5003_data_t pms_data;
    err = pms5003_get_data(&pms_data);
    if (!err && pms_data.checksum_ok)
    {    
        // create the pms5003 root
        cJSON *pms_root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, JSON_ROOT_PMS, pms_root);

        // add the pms5003 data to their root
        cJSON_AddNumberToObject(pms_root, PMS_PM1_KEY, pms_data.concAtm.pm1);
        cJSON_AddNumberToObject(pms_root, PMS_PM2_5_KEY, pms_data.concAtm.pm2_5);
        cJSON_AddNumberToObject(pms_root, PMS_PM10_KEY, pms_data.concAtm.pm10);
    }
#endif // USE_PMS5003

#ifdef USE_SPH0645
    // get the sph0645 data
    sph0645_data_t sph_data;
    err = sph0645_get_data(&sph_data);
    if (!err)
    {
        // create the sph0645 root
        cJSON *sph_root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, JSON_ROOT_SPH, sph_root);

        // add the sph0645 data to their root
        cJSON_AddNumberToObject(sph_root, SPH_AVG_KEY, TRUNC2(sph_data.avg));
        cJSON_AddNumberToObject(sph_root, SPH_MIN_KEY, TRUNC2(sph_data.min));
        cJSON_AddNumberToObject(sph_root, SPH_MAX_KEY, TRUNC2(sph_data.max));
    }
#endif // USE_SPH0645

    return ESP_OK;
}

esp_err_t sensors_clear_data(cJSON *root)
{
#ifdef USE_SPH0645
    sph0645_clear_data();
#endif // USE_SPH0645

    // append a return value to the root
    cJSON_AddBoolToObject(root, MQTT_RESET_DATA_KEY, cJSON_True);

    return ESP_OK;
}