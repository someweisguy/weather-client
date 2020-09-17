#pragma once

#include "esp_system.h"

typedef struct pms5003_data
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
    int64_t up_time;
} pms5003_data_t;

esp_err_t pms5003_start();

esp_err_t pms5003_set_power(uint32_t level);

esp_err_t pms5003_get_data(pms5003_data_t *data);