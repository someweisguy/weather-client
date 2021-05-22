#pragma once

#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"

class sensor_t {
protected:
  const char *name;

public:
  sensor_t(const char* name) : name(name) {
    // do nothing...
  }

  const char *get_name() const {
    return name;
  }

  virtual int get_discovery(const discovery_t *&discovery) const = 0;

  virtual esp_err_t setup() = 0;

  virtual esp_err_t ready() {
    return ESP_OK;
  }

  virtual esp_err_t get_data(cJSON *json) = 0;

  virtual esp_err_t sleep() {
    return ESP_OK;
  }
};