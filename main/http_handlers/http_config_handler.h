#pragma once
#include "esp_system.h"
#include "esp_http_server.h"

#include "cJSON.h"

#include "max17043.h"
#include "pms5003.h"
#include "bme280.h"
#include "sph0645.h"

#include "json_keys.h"

#define HTTPD_507 "507 Insufficient Storage"

esp_err_t config_handler(const char *request)
{
    // parse the JSON object
    cJSON *root = cJSON_Parse(request);
    if (root == NULL)
        return ESP_ERR_INVALID_ARG;

    // iterate the JSON object
    esp_err_t err = ESP_OK;
    cJSON *device;
    cJSON_ArrayForEach(device, root)
    {
        // edit bme280 config
        if (strcasecmp(device->string, JSON_ROOT_BME) == 0)
        {
            bme280_config_t config;
            err = bme280_get_config(&config);
            if (err)
                break;

            cJSON *elem;
            cJSON_ArrayForEach(elem, root->child)
            {
                if (strcasecmp(elem->string, BME_OVERSAMPLING_KEY) == 0)
                {
                    cJSON *osrs; // oversampling json nodes
                    cJSON_ArrayForEach(osrs, elem->child)
                    {
                        if (strcasecmp(osrs->string, BME_TEMPERATURE_KEY) == 0)
                            config.ctrl_meas.osrs_t = elem->valueint;
                        else if (strcasecmp(osrs->string, BME_HUMIDITY_KEY) == 0)
                            config.ctrl_hum.osrs_h = elem->valueint;
                        else if (strcasecmp(osrs->string, BME_PRESSURE_KEY) == 0)
                            config.ctrl_meas.osrs_p = elem->valueint;
                    }
                }
                else if (strcasecmp(elem->string, BME_MODE_KEY) == 0)
                    config.ctrl_meas.mode = elem->valueint;
                else if (strcasecmp(elem->string, BME_STANDBY_TIME_KEY) == 0)
                    config.config.t_sb = elem->valueint;
                else if (strcasecmp(elem->string, BME_FILTER_KEY) == 0)
                    config.config.filter = elem->valueint;
            }

            // set bme280 elevation
            elem = cJSON_GetObjectItem(root, BME_ELEVATION_KEY);
            if (elem != NULL)
                bme280_set_elevation(elem->valueint);

            err = bme280_set_config(&config);
            if (err)
                break;
        }

        // edit pms5003 config
        else if (strcasecmp(device->string, JSON_ROOT_PMS) == 0)
        {
            // get the current config
            pms5003_config_t config;
            err = pms5003_get_config(&config);
            if (err)
                break;

            // set the config to the requested mode
            cJSON *config_node = cJSON_GetObjectItem(root, JSON_CONFIG_KEY);
            if (config_node != NULL)
            {
                cJSON *elem;
                cJSON_ArrayForEach(elem, config_node)
                {
                    if (strcasecmp(elem->string, PMS_MODE_KEY) == 0)
                        config.mode = elem->valueint;
                    else if (strcasecmp(elem->string, PMS_SLEEP_SET_KEY) == 0)
                        config.sleep = elem->valueint;
                }
            }

            // return results
            err = pms5003_set_config(&config);
            if (err)
                break;
        }

        // edit sph0645 config
        else if (strcasecmp(device->string, JSON_ROOT_SPH) == 0)
        {
            // get the current config
            sph0645_config_t config;
            err = sph0645_get_config(&config);
            if (err)
                break;

            bool config_changed = false; // avoid unneccessary task restarts

            // set the config to the requested mode
            cJSON *elem;
            cJSON_ArrayForEach(elem, root->child)
            {
                if (strcasecmp(elem->string, SPH_SAMPLE_LEN_KEY) == 0)
                {
                    if (elem->valueint != config.sample_length)
                        config_changed = true;
                    config.sample_length = elem->valueint;
                }
                else if (strcasecmp(elem->string, SPH_SAMPLE_PERIOD_KEY) == 0)
                {
                    if (elem->valueint != config.sample_period)
                        config_changed = true;
                    config.sample_period = elem->valueint;
                }
                else if (strcasecmp(elem->string, SPH_SAMPLE_WEIGHTING_KEY) == 0)
                {
                    if (elem->valueint != config.weighting)
                        config_changed = true;
                    config.weighting = elem->valueint;
                }
            }

            // get the clear data key if it exists
            elem = cJSON_GetObjectItem(root, SPH_CLEAR_DATA_KEY);
            if (elem != NULL && cJSON_IsTrue(elem))
                sph0645_clear_data();

            if (config_changed)
                err = sph0645_set_config(&config);
            if (err)
                break;
        }
    }

    cJSON_Delete(root);

    return err;
}

esp_err_t http_config_handler(httpd_req_t *r)
{
    // set the response type
    httpd_resp_set_type(r, HTTPD_TYPE_TEXT);

    // read the body of the request into memory
    char *request = malloc(r->content_len + 1); // content + null terminator
    if (request == NULL)
    {
        httpd_resp_set_status(r, HTTPD_507);
        httpd_resp_sendstr(r, "NO MEMORY");
        return ESP_ERR_NO_MEM;
    }
    httpd_req_recv(r, request, r->content_len + 1);

    esp_err_t err = config_handler(request);
    free(request);

    // send a response to the client
    if (err)
    {
        if (err == ESP_ERR_INVALID_ARG)
            httpd_resp_set_status(r, HTTPD_400);
        else
            httpd_resp_set_status(r, HTTPD_500);
        httpd_resp_sendstr(r, "FAIL");
    }
    else
    {
        httpd_resp_set_status(r, HTTPD_200);
        httpd_resp_sendstr(r, "OK");
    }

    return ESP_OK;
}