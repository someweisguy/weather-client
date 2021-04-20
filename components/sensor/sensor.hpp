#pragma once

#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"

class Sensor {
protected:
  discovery_t *discovery;
  const char *name;

public:
  Sensor(const char* name) : name(name) {
    // do nothing...
  }

  ~Sensor() {
    delete[] discovery;
  }

  const char *get_name() const {
    return name;
  }

  int get_discovery(discovery_t *&discovery) {
    discovery = (this->discovery);
    return sizeof(this->discovery);
  }

  virtual esp_err_t setup() {
    return ESP_OK;
  }

  virtual esp_err_t ready() {
    return ESP_OK;
  }

  virtual esp_err_t get_data(cJSON *json) {
    return ESP_OK;
  }

  virtual esp_err_t sleep() {
    return ESP_OK;
  }
};