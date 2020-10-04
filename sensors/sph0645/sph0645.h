#pragma once

#include "esp_system.h"

#define SPH0645_WEIGHTING_NONE BIT(0)
#define SPH0645_WEIGHTING_C BIT(1)
#define SPH0645_WEIGHTING_A BIT(2)

typedef struct
{
    float avg;
    float min;
    float max;
    uint64_t samples;
} sph0645_data_t;

typedef struct
{
    uint32_t sample_length; // Length of time in which audio samples are taken (ms).
    uint32_t sample_period; // Period in which audio values are calculated (ms).
    uint8_t weighting;      // Decibel weighting of the collected waveform.
} sph0645_config_t;

#define SPH0645_DEFAULT_CONFIG                                                        \
    {                                                                                 \
        .sample_length = 125, .sample_period = 1000, .weighting = SPH0645_WEIGHTING_C \
    }

esp_err_t sph0645_reset();

esp_err_t sph0645_set_config(const sph0645_config_t *config);
esp_err_t sph0645_get_config(sph0645_config_t *config);

esp_err_t sph0645_get_data(sph0645_data_t *data);

void sph0645_clear_data();