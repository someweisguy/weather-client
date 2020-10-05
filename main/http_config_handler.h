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

static esp_err_t bme280_config_edit(cJSON *root)
{
    // get the current config
    bme280_config_t config;
    esp_err_t err = bme280_get_config(&config);
    if (err)
        return err;

    cJSON *elem;
    cJSON_ArrayForEach(elem, root->child)
    {
        // TODO
    }

    // set bme280 elevation
    elem = cJSON_GetObjectItem(root, BME_ELEVATION_KEY);
    if (elem != NULL)
    {
        bme280_set_elevation(elem->valueint);
    }

    return bme280_set_config(&config);
}

static esp_err_t pms5003_config_edit(cJSON *root)
{
    // get the current config
    pms5003_config_t config;
    esp_err_t err = pms5003_get_config(&config);
    if (err)
        return err;

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
    return pms5003_set_config(&config);
}

static esp_err_t sph0645_config_edit(cJSON *root)
{
    // get the current config
    sph0645_config_t config;
    esp_err_t err = sph0645_get_config(&config);
    if (err)
        return err;

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
    if (elem != NULL)
    {
        if (elem->valueint == 1)
            sph0645_clear_data();
        else
            return ESP_ERR_INVALID_ARG;
    }

    return config_changed ? sph0645_set_config(&config) : ESP_OK;
}

esp_err_t http_config_handler(httpd_req_t *r)
{
    // set the response type
    httpd_resp_set_type(r, HTTPD_TYPE_TEXT);

    // read the body of the request into memory
    char *body = malloc(r->content_len + 1); // content + null terminator
    if (body == NULL)
    {
        httpd_resp_set_status(r, HTTPD_507);
        httpd_resp_sendstr(r, "NO MEMORY");
        return ESP_ERR_NO_MEM;
    }
    httpd_req_recv(r, body, r->content_len + 1);

    // parse the JSON object
    cJSON *root = cJSON_Parse(body);
    if (root == NULL)
    {
        httpd_resp_set_status(r, HTTPD_400);
        httpd_resp_sendstr(r, "BAD JSON");
        free(body);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    do
    {
        cJSON *device;
        cJSON_ArrayForEach(device, root)
        {
            if (strcasecmp(device->string, JSON_ROOT_BME) == 0)
            {
                err = bme280_config_edit(device);
                if (err)
                    break;
            }
            else if (strcasecmp(device->string, JSON_ROOT_PMS) == 0)
            {
                err = pms5003_config_edit(device);
                if (err)
                    break;
            }
            else if (strcasecmp(device->string, JSON_ROOT_SPH) == 0)
            {
                err = sph0645_config_edit(device);
                if (err)
                    break;
            }
        }
    } while (false);

    // send a response to the client
    if (err == ESP_ERR_INVALID_ARG)
    {
        httpd_resp_set_status(r, HTTPD_400);
        httpd_resp_sendstr(r, "FAIL");
    }
    else if (err)
    {
        httpd_resp_set_status(r, HTTPD_500);
        httpd_resp_sendstr(r, "FAIL");
    }
    else
    {
        httpd_resp_set_status(r, HTTPD_200);
        httpd_resp_sendstr(r, "OK");
    }

    // free resources
    cJSON_Delete(root);
    free(body);

    return ESP_OK;
}