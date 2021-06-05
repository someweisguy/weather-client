#pragma once

#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "wireless.h"

typedef struct {
  const char *name;
  esp_err_t err;
  int msg_id;
  cJSON *payload;
} sensor_data_t;

class sensor_t {
protected:
  const char *name;
  const discovery_t *discoveries;
  const size_t num_discoveries;

public:
  sensor_t(const char* name, const discovery_t discoveries[], 
      size_t num_discoveries) : name(name), discoveries(discoveries),
      num_discoveries(num_discoveries) {
  }

  const char *get_name() const {
    return name;
  }

  int get_discovery(const discovery_t *&discoveries) const {
    discoveries = this->discoveries;
    return num_discoveries;
  }

  virtual esp_err_t setup() = 0;

  virtual esp_err_t ready() {
    return ESP_OK;
  }

  virtual esp_err_t get_data(cJSON *json) = 0;

  virtual esp_err_t sleep() {
    return ESP_OK;
  }
};