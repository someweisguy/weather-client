#pragma once

#include "esp_system.h"

#define PMS5003_WAKEUP 1  // PMS5003 wakeup.
#define PMS5003_SLEEP 0   // PMS5003 sleep.
#define PMS5003_ACTIVE 1  // PMS5003 active mode.
#define PMS5003_PASSIVE 0 // PMS5003 passive mode.

typedef struct
{
    struct
    {
        uint16_t pm1;
        uint16_t pm2_5;
        uint16_t pm10;
    } concCF1;
    struct
    {
        uint16_t pm1;
        uint16_t pm2_5;
        uint16_t pm10;
    } concAtm;
    struct
    {
        uint16_t um0_3;
        uint16_t um0_5;
        uint16_t um1_0;
        uint16_t um2_5;
        uint16_t um5_0;
        uint16_t um10_0;
    } countPer0_1L;
    bool checksum_ok;
    int64_t fan_on_time;
} pms5003_data_t;

typedef struct 
{
    uint8_t mode;
    uint8_t sleep;
} pms5003_config_t;

#define PMS5003_ACTIVE_AWAKE { .mode = PMS5003_ACTIVE, .sleep = PMS5003_WAKEUP }
#define PMS5003_ACTIVE_ASLEEP { .mode = PMS5003_ACTIVE, .sleep = PMS5003_SLEEP }
#define PMS5003_PASSIVE_AWAKE { .mode = PMS5003_PASSIVE, .sleep = PMS5003_WAKEUP }
#define PMS5003_PASSIVE_ASLEEP { .mode = PMS5003_PASSIVE, .sleep = PMS5003_SLEEP }

esp_err_t pms5003_reset();

esp_err_t pms5003_get_config(pms5003_config_t *config);
esp_err_t pms5003_set_config(const pms5003_config_t *config);

esp_err_t pms5003_get_data(pms5003_data_t *data);

esp_err_t pms5003_get_power(uint32_t *level);
esp_err_t pms5003_set_power(uint32_t level);