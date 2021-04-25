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
#include "pms5003.hpp"
#include "max17043.hpp"
#include "sph0645.hpp"

#define SYSTEM_KEY          "system"
#define WIRELESS_KEY        "wireless"
#define SIGNAL_STRENGTH_KEY "signal_strength"
#define RESET_REASON_KEY    "reset_reason"

static const char *TAG = "main";
RTC_DATA_ATTR bool device_is_setup;
RTC_DATA_ATTR double latitude, longitude, elevation_m;
RTC_DATA_ATTR time_t last_time_sync_ts;

static Sensor *sensors[] = { new bme280_t(0x76, elevation_m), 
  new pms5003_t(), new max17043_t(0x36), new sph0645_t() };

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
  // TODO: move individual serial start functions to sensor init
  esp_err_t err = serial_start();
  if (err) {
    ESP_LOGE(TAG, "Unable to start serial drivers. Restarting...");
    esp_restart();
  }

  if (!device_is_setup) {
    ESP_LOGI(TAG, "Doing initial device setup");

    ESP_LOGI(TAG, "Connecting to wireless services...");
    xTaskCreate(wireless_connect_task, "wireless_connect_task", 4096,
      xTaskGetCurrentTaskHandle(), 0, nullptr);

    ESP_LOGI(TAG, "Initializing sensors...");
    for (Sensor *sensor : sensors) {
      err = sensor->setup();
      if (err) {
        // TODO: will this boot loop?
        ESP_LOGE(TAG, "An error occurred initializing sensors. Restarting...");
        esp_restart();
      }
    }

    // wait for wifi to connect
    xTaskNotifyWait(0, -1, reinterpret_cast<uint32_t *>(&err), portMAX_DELAY);
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
    err = wireless_get_location(&latitude, &longitude, &elevation_m);
    if (err) {
      ESP_LOGE(TAG, "Unable to get location. Restarting...");
      esp_restart();
    }

    // declare default discovery for device
    const discovery_t default_discoveries[] = {
      {
        .topic = "sensor/signal_strength",
        .config = {
          .device_class = "signal_strength",
          .expire_after = 310,
          .force_update = true,
          .icon = nullptr,
          .name = "Signal Strength",
          .unit_of_measurement = "dB",
          .value_template = "{{ value_json." WIRELESS_KEY "." SIGNAL_STRENGTH_KEY " }}"
        },
      }
    };

    // send default discovery
    for (discovery_t discovery : default_discoveries) {
      err = wireless_discover(&discovery, 1, false, 15000 / portTICK_PERIOD_MS);
      if (err) {
        ESP_LOGE(TAG, "An error occurred sending discovery. Restarting...");
        esp_restart();
      }
    }

    // send discovery for each sensor
    for (Sensor *sensor : sensors) {
      // publish each discovery topic
      const discovery_t *discoveries;
      const int num_discoveries = sensor->get_discovery(discoveries);
      ESP_LOGD(TAG, "The %s has %i discoveries.", sensor->get_name(), 
        num_discoveries);
      for (int i = 0; i < num_discoveries; ++i) { 
        err = wireless_discover(&(discoveries[i]), 1, false, 
          15000 / portTICK_PERIOD_MS);
        if (err) {
          ESP_LOGE(TAG, "An error occurred sending discovery. Restarting...");
          esp_restart();
        }
      }
    }

    // build system data json object
    cJSON *json = cJSON_CreateObject(), *system = cJSON_CreateObject();
    cJSON_AddNumberToObject(system, RESET_REASON_KEY, esp_reset_reason());
    cJSON_AddItemToObject(json, SYSTEM_KEY, system);

    // get wifi signal strength
    int signal_strength;
    err = wireless_get_rssi(&signal_strength);
    if (!err) {
      cJSON *wireless = cJSON_CreateObject();
      cJSON_AddNumberToObject(wireless, SIGNAL_STRENGTH_KEY, signal_strength);
      cJSON_AddItemToObject(json, WIRELESS_KEY, wireless);
    }

    // publish the setup data
    err = wireless_publish_data(json, 2, false, 15000 / portTICK_PERIOD_MS);
    if (err) {
      ESP_LOGE(TAG, "An error occurred sending setup data. Restarting...");
      esp_restart();
    }
    wireless_stop(5000 / portTICK_PERIOD_MS);

    cJSON_Delete(json);
    device_is_setup = true;
  } else {
    ESP_LOGI(TAG, "Readying sensors");
    for (Sensor *sensor : sensors) {
      err = sensor->ready();
      if (err) {
        ESP_LOGE(TAG, "An error occurred readying the %s.", 
          sensor->get_name());
      }
    }

    // wait until 15s before measurement to start wifi and mqtt
    const int wifi_time_ms = 15 * 1000;
    int wait_time_ms = (time_to_next_state_us() / 1000) - wifi_time_ms;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);

    // connect to wifi and mqtt
    xTaskCreate(wireless_connect_task, "wireless_connect_task", 4096,
      xTaskGetCurrentTaskHandle(), 0, nullptr);

    // create json json payload
    cJSON *json = cJSON_CreateObject();

    // wait until measurement window
    wait_time_ms = time_to_next_state_us() / 1000;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);

    // get sensor data
    ESP_LOGI(TAG, "Getting sensor data...");
    for (Sensor *sensor : sensors) {
      err = sensor->get_data(json);
      if (err) {
        ESP_LOGE(TAG, "An error occurred getting data from %s.", 
          sensor->get_name());
      }
    }

    // put sensors to sleep
    for (Sensor *sensor : sensors) {
      err = sensor->sleep();
      if (err) { 
        ESP_LOGE(TAG, "An error occurred putting the %s to sleep.", 
          sensor->get_name());
      }
    }

    // wait until wifi is connected
    xTaskNotifyWait(0, -1, reinterpret_cast<uint32_t *>(&err), portMAX_DELAY);
    if (!err) {
      // get information about the wifi signal strength
      int signal_strength;
      err = wireless_get_rssi(&signal_strength);
      if (!err) {
        cJSON *wireless = cJSON_CreateObject();
        cJSON_AddNumberToObject(wireless, SIGNAL_STRENGTH_KEY, signal_strength);
        cJSON_AddItemToObject(json, WIRELESS_KEY, wireless);
      }

      // publish json to mqtt broker
      wireless_publish_data(json, 0, false, 0);
      wireless_stop(10000 / portTICK_PERIOD_MS);
    }

    // delete the json object
    cJSON_Delete(json);

    // check if the clock needs to be resynchronized
    const int time_sync_period_s = 89400;
    if (time(nullptr) > last_time_sync_ts + time_sync_period_s) {
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
