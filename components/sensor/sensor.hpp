#pragma once

#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"

#define JSON_GET(value) #value
#define JSON_KV(key, value) "\"" #key "\": " #value "," 

class Sensor {
protected:
  // TODO: name
  int num_discover_strings;
  const char **discover_topics;
  cJSON **discover_strings;

  void init_sensor_discovery(cJSON* json, int expire_after, bool force_update, 
      const char *icon, const char *name, int qos, const char *state_topic, 
      const char *unique_id, const char *unit_of_measurement, 
      const char *value_template) {
    cJSON_AddNumberToObject(json, "expire_after", expire_after);
    cJSON_AddBoolToObject(json, "force_update", force_update);
    cJSON_AddStringToObject(json, "icon", icon);
    cJSON_AddStringToObject(json, "name", name);
    cJSON_AddNumberToObject(json, "qos", qos);
    cJSON_AddStringToObject(json, "state_topic", state_topic);
    cJSON_AddStringToObject(json, "unique_id", unique_id);
    cJSON_AddStringToObject(json, "unit_of_measurement", unit_of_measurement);
    cJSON_AddStringToObject(json, "value_template", value_template);
  }

public:
  Sensor(const int num_discover_strings) : num_discover_strings(num_discover_strings) {
    this->discover_topics = new const char*[this->num_discover_strings];
    this->discover_strings = new cJSON*[this->num_discover_strings];
    for (int i = 0; i < num_discover_strings; ++i) {
      this->discover_topics[i] = nullptr;
      this->discover_topics[i] = nullptr;
    }
  }

  ~Sensor() {
    delete[] this->discover_topics;
    delete[] this->discover_strings;
  }

  int get_discovery(const char **&config_topic, cJSON **&json) const {
    config_topic = (this->discover_topics);
    json = (this->discover_strings);

    // check that the discover strings are set up properly
    int actual_num = this->num_discover_strings;
    for (int i = 0; i < this->num_discover_strings; ++i) {
      if (this->discover_topics[i] == nullptr || this->discover_strings[i] == nullptr) {
        actual_num = i;
        ESP_LOGW("sensor", "Discovery strings not setup in sensor!");
        break;
      }
    }

    return actual_num;
  }

  virtual esp_err_t setup() const {
    return ESP_OK;
  }


  virtual esp_err_t get_data(cJSON *json) const {
    return ESP_OK;
  }
};