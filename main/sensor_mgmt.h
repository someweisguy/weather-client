#pragma once
#include "esp_system.h"
#include "cJSON.h"

void sensors_start(cJSON *json);

void sensors_wakeup(cJSON *json);

void sensors_get_data(cJSON *json);

void sensors_sleep(cJSON *json);