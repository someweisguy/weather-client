#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_system.h"

float audio_equalize(float *input, float *output, size_t len);

float audio_weight_dBC(float *input, float *output, size_t len);

#ifdef __cplusplus
}
#endif