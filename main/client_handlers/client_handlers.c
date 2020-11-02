#include "client_handlers.h"

#include <string.h>
#include <math.h>
#include "cJSON.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#define TRUNC2(n) (int)(n * 100) / 100.0

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
    esp_err_t err; 
    
    /*
    // get the wlan data
    wlan_data_t wlan_data;
    esp_err_t err = wlan_get_data(&wlan_data);
    if (err)
        return err;
    cJSON_AddNumberToObject(system_root, SYSTEM_WIFI_RSSI_KEY, wlan_data.rssi);
    */

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