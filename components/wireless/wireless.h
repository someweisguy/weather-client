#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

typedef struct {
  const char *topic;
  struct {
      const char *device_class;
      bool force_update;
      const char *icon;
      const char *name;
      const char *unit_of_measurement;
      const char *value_template;
  } config;
} discovery_t;

esp_err_t wireless_start(const char *ssid, const char *password,
    const char *broker, TickType_t timeout);

esp_err_t wireless_stop(TickType_t timeout);

esp_err_t wireless_synchronize_time(const char *server, TickType_t timeout);

esp_err_t wireless_get_location(double *latitude, double *longitude, 
    double *elevation_m);

esp_err_t wireless_get_rssi(int *rssi);

esp_err_t wireless_publish_discover(const char *sensor_name, 
    const discovery_t *discovery);

esp_err_t wireless_publish_state(const char *sensor_name, 
    cJSON *payload);

esp_err_t wireless_wait_for_publish(TickType_t timeout);

#ifdef __cplusplus
}
#endif