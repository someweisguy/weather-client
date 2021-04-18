#include "cJSON.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/time.h>

#include "secrets.h"
#include "serial.h"
#include "wireless.h"
#include "sensor.hpp"
#include "bme280.hpp"

static const char *TAG = "main";
RTC_DATA_ATTR bool device_is_setup;
RTC_DATA_ATTR double latitude, longitude, elevation;
RTC_DATA_ATTR time_t last_time_sync_ts;

static Sensor *sensors[] = { new bme280_t(0x76, elevation)};

static void wireless_connect_task(void *args) {
  const TaskHandle_t calling_task = args;
  const esp_err_t err = wireless_start(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER,
    15000 / portTICK_PERIOD_MS);
  xTaskNotify(calling_task, err, eSetValueWithOverwrite);
  vTaskDelete(NULL);
}

static inline int time_to_next_state_us() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((300 - (tv.tv_sec % 300) - 1) * 1e6) + (1e6 - tv.tv_usec);
}

extern "C" void app_main(void) {
  // initialize serial services
  esp_err_t err = serial_start();
  if (err) {
    ESP_LOGE(TAG, "Unable to start serial drivers. Restarting...");
    esp_restart();
  }

  if (!device_is_setup) {
    ESP_LOGI(TAG, "Doing initial device setup");

    ESP_LOGI(TAG, "Connecting to wireless services...");
    xTaskCreate(wireless_connect_task, "wireless_connect_task", 4096,
      xTaskGetCurrentTaskHandle(), 0, NULL);

    ESP_LOGI(TAG, "Initializing sensors...");
    for (const Sensor *sensor : sensors) {
      err = sensor->setup();
      if (err) {
        ESP_LOGE(TAG, "An error occurred initializing sensors. Restarting...");
        esp_restart();
      }
    }

    // wait for wifi to connect
    xTaskNotifyWait(0, -1, (uint32_t *)&err, portMAX_DELAY);
    if (err) {
      ESP_LOGE(TAG, "Unable to connect to wireless services. Restarting...");
      esp_restart();
    }

    ESP_LOGI(TAG, "Synchronizing time...");
    err = wireless_synchronize_time(SNTP_SERVER, 15000 / portTICK_PERIOD_MS);
    if (err) {
      ESP_LOGE(TAG, "Unable to synchronize time with SNTP server. Restarting...");
      esp_restart();
    }
    time(&last_time_sync_ts);

    ESP_LOGI(TAG, "Getting location...");
    err = wireless_get_location(&latitude, &longitude, &elevation);
    if (err) {
      ESP_LOGE(TAG, "Unable to get location. Restarting...");
      esp_restart();
    }
    
    ESP_LOGI(TAG, "Sending discovery MQTT strings...");
    for (const Sensor *sensor : sensors) {
      cJSON **json_objects;
      const char **config_topics;
      const int num_topics = sensor->get_discovery(config_topics, json_objects);
      // attach device JSON
      for (int i = 0; i < num_topics; ++i) {
        ESP_LOGI(TAG, "Config Topic %i", i);
        err = wireless_publish(config_topics[i], json_objects[i], 2, false, 
          5000 / portTICK_PERIOD_MS);
        if (err) {
          ESP_LOGE(TAG, "An error occurred sending discovery. Restarting...");
          esp_restart();
        }
      }
    }

    // send lat/long/elev and restart reason
    const esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %i", reset_reason);
    ESP_LOGI(TAG, "Latitude: %.2f, Longitude: %.2f, Elevation: %.2fm",
      latitude, longitude, elevation);

    // build data json object
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "latitude", latitude);
    cJSON_AddNumberToObject(data, "longitude", longitude);
    cJSON_AddNumberToObject(data, "elevation", elevation * 3.28084);
    cJSON_AddNumberToObject(data, "reset", reset_reason);

    wireless_publish(DATA_TOPIC, data, 2, false, 5000 / portTICK_PERIOD_MS);

    wireless_stop(5000 / portTICK_PERIOD_MS);

    device_is_setup = true;
  } else {
    ESP_LOGI(TAG, "Device is awake");

    // wake up sensors
    // TODO

    // wait until 15s before measurement to start wifi and mqtt
    const int wifi_time_ms = 15 * 1000;
    int wait_time_ms = (time_to_next_state_us() / 1000) - wifi_time_ms;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);

    // connect to wifi and mqtt
    xTaskCreate(wireless_connect_task, "wireless_connect_task", 4096,
      xTaskGetCurrentTaskHandle(), 0, NULL);

    // create json payload
    cJSON *json = cJSON_CreateObject();

    // wait until measurement window
    wait_time_ms = time_to_next_state_us() / 1000;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);

    // get sensor data
    ESP_LOGI(TAG, "Getting sensor data...");
    // TODO

    // wait until wifi is connected
    xTaskNotifyWait(0, -1, (uint32_t *)&err, portMAX_DELAY);
    if (!err) {
      // get information about the wifi connection
      wireless_get_data(json);

      // publish data to mqtt broker
      wireless_publish(DATA_TOPIC, json, 2, false, 10000 / portTICK_PERIOD_MS);
    }

    // delete the json object
    cJSON_Delete(json);

    // check if the clock needs to be resynchronized
    const int time_sync_period_s = 89400;
    if (time(NULL) > last_time_sync_ts + time_sync_period_s) {
      ESP_LOGI(TAG, "Re-synchronizing time...");
      err = wireless_synchronize_time(SNTP_SERVER, 15000 / portTICK_PERIOD_MS);
      if (err) {
        ESP_LOGE(TAG, "Unable to synchronize time with SNTP server. Restarting...");
        esp_restart();
      }
      time(&last_time_sync_ts);
    }
  }

  ESP_LOGI(TAG, "Going to sleep...");
  const int wake_early_us = 32 * 1e6; 
  int sleep_time_us = time_to_next_state_us() - wake_early_us;
  if (sleep_time_us < 10000) {
    // not enough time - skip this measurement period
    sleep_time_us += 5 * 60 * 1e6;
  }

  esp_deep_sleep(sleep_time_us);
}
