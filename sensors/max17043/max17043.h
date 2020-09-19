#pragma once

#include "esp_system.h"

#define MAX17043_QUICKSTART_COMMAND 0x4000

#define MAX17043_DEFAULT_CONFIG { .config = { .val = 0x971c }, .mode = 0 }

typedef struct
{
    float millivolts;
    float battery_life;
} max17043_data_t;

typedef struct
{
    union
    {
        struct
        {
            uint16_t athd : 5; // Alert Threshold. The alert threshold is a 5-bit value that sets the state of charge level where an interrupt is generated on the ALRT pin. The alert threshold has an LSb weight of 1% and can be programmed from 1% up to 32%. The threshold value is stored in two’s-complement form (00000 = 32%, 00001 = 31%, 00010 = 30%, 11111 = 1%). The power-up default value for ATHD is 4% or 1Ch.
            uint16_t alrt : 1; // ALERT Flag. This bit is set by the IC when the SOC register value falls below the alert threshold setting and an interrupt is generated. This bit can only be cleared by software. The power-up default value for ALRT is logic 0.
            uint16_t : 1;
            uint16_t sleep : 1; // Sleep Bit. Writing SLEEP to logic 1 forces the ICs into Sleep mode. Writing SLEEP to logic 0 forces the ICs to exit Sleep mode. The power-up default value for SLEEP is logic 0.
            uint16_t rcomp : 8;
        };
        uint16_t val;
    } config;      // CONFIG is an 8-bit value that can be adjusted to optimize IC performance for different lithium chemistries or different operating temperatures. Contact Maxim for instructions for optimization. The power-up default value for CONFIG is 97h.
    uint16_t mode; // The MODE register allows the host processor to send special commands to the IC (Table 2). Valid MODE register write values are listed as follows. All other MODE register values are reserved.
} max17043_config_t;

esp_err_t max17043_reset();

esp_err_t max17043_set_config(const max17043_config_t *config);
esp_err_t max17043_get_config(max17043_config_t *config);

esp_err_t max17043_get_data(max17043_data_t *data);

uint8_t max17043_alert_threshold(uint8_t percentage);

esp_err_t max17043_get_version(uint16_t *version);