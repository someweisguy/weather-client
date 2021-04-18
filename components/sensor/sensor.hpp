#pragma once

#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"

#define JSON_GET(value) #value
#define JSON_KV(key, value) "\"" #key "\": " #value "," 

class Sensor {
private:
  const char **config_topics;
  cJSON **config_jsons;

protected:
  struct discovery_t {
    const char *topic;
    struct {
      int expire_after;
      bool force_update;
      const char *icon;
      const char *name;
      int qos;
      const char *state_topic;
      const char *unique_id;
      const char *unit_of_measurement;
      const char *value_template;
    } config;
  };
  const discovery_t *discovery;
  const char *name;

public:
  Sensor(const char* name) : name(name) {
    // do nothing...
  }

  ~Sensor() {
    delete[] this->discovery;
    if (this->config_topics) delete[] this->config_topics;
    if (this->config_jsons) delete[] this->config_jsons;
  }

  const char *get_name() const {
    return this->name;
  }

  int get_discovery(const char **&config_topics, cJSON **&config_jsons) {
    // get the number of discovery topics
    const int num_configs = sizeof(this->discovery);
    
    // allocate space for discovery topics and json objects
    this->config_topics = new const char*[num_configs];
    this->config_jsons = new cJSON*[num_configs];

    for (int i = 0; i < num_configs; ++i) {
      // copy the pointer to the discovery topic to the topic buffer
      this->config_topics[i] = this->discovery[i].topic;

      // create the json object and fill it with required params
      this->config_jsons[i] = cJSON_CreateObject();
      cJSON_AddNumberToObject(this->config_jsons[i], "expire_after", this->discovery[i].config.expire_after);
      cJSON_AddBoolToObject(this->config_jsons[i], "force_update", this->discovery[i].config.force_update);
      cJSON_AddStringToObject(this->config_jsons[i], "icon", this->discovery[i].config.icon);
      cJSON_AddStringToObject(this->config_jsons[i], "name", this->discovery[i].config.name);
      cJSON_AddNumberToObject(this->config_jsons[i], "qos", this->discovery[i].config.qos);
      cJSON_AddStringToObject(this->config_jsons[i], "state_topic", this->discovery[i].config.state_topic);
      cJSON_AddStringToObject(this->config_jsons[i], "unique_id", this->discovery[i].config.unique_id);
      cJSON_AddStringToObject(this->config_jsons[i], "unit_of_measurement", this->discovery[i].config.unit_of_measurement);
      cJSON_AddStringToObject(this->config_jsons[i], "value_template", this->discovery[i].config.value_template);
    }

    // copy the pointers to the arg references and return num_configs
    config_topics = this->config_topics;
    config_jsons = this->config_jsons;
    return num_configs;
  }

  virtual esp_err_t setup() {
    return ESP_OK;
  }

  virtual esp_err_t wake_up() {
    return ESP_OK;
  }

  virtual esp_err_t get_data(cJSON *json) {
    return ESP_OK;
  }

  virtual esp_err_t sleep() {
    return ESP_OK;
  }
};