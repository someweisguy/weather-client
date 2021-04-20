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
RTC_DATA_ATTR double latitude, longitude, elevation_m;
RTC_DATA_ATTR time_t last_time_sync_ts;

static Sensor *sensors[] = { new bme280_t(0x76, elevation_m)};

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

  // for (Sensor *sensor : sensors) {
  //   sensor->setup();
  //   vTaskDelay(1);
  //   sensor->ready();
  // }

  // vTaskDelay(1);

  // ESP_LOGI(TAG, "gettint data)");
  // cJSON *j = cJSON_CreateObject();
  // for (Sensor *sensor : sensors) {
  //   sensor->get_data(j);
  // }
  // char *c = cJSON_Print(j);
  // ESP_LOGI(TAG, "%s", c);

  // vTaskDelay(15000 / portTICK_PERIOD_MS);


  if (!device_is_setup) {
    ESP_LOGI(TAG, "Doing initial device setup");

    ESP_LOGI(TAG, "Connecting to wireless services...");
    xTaskCreate(wireless_connect_task, "wireless_connect_task", 4096,
      xTaskGetCurrentTaskHandle(), 0, nullptr);

    ESP_LOGI(TAG, "Initializing sensors...");
    for (Sensor *sensor : sensors) {
      err = sensor->setup();
      if (err) {
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

    // build the device config for discovery
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "manufacturer", "Mitch Weisbrod");
    // TODO: identifiers, model, name, sw_version
    
    ESP_LOGI(TAG, "Sending discovery MQTT strings...");
    for (Sensor *sensor : sensors) {
      // publish each discovery topic
      discovery_t *discoveries;
      const int num_discoveries = sensor->get_discovery(discoveries);
      for (int i = 0; i < num_discoveries; ++i) { 
        err = wireless_discover(&(discoveries[i]), 2, false, 
          5000 / portTICK_PERIOD_MS);
        if (err) {
          ESP_LOGE(TAG, "An error occurred sending discovery. Restarting...");
          esp_restart();
        }
      }
    }
    cJSON_Delete(device);

    // send lat/long/elev and restart reason
    const esp_reset_reason_t reset_reason = esp_reset_reason();
    const double elevation_ft = elevation_m * 3.28084;
    ESP_LOGI(TAG, "Reset reason: %i", reset_reason);
    ESP_LOGI(TAG, "Latitude: %.2f, Longitude: %.2f, Elevation: %.2f ft",
      latitude, longitude, elevation_ft);

    // build data json object
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "latitude", latitude);
    cJSON_AddNumberToObject(data, "longitude", longitude);
    cJSON_AddNumberToObject(data, "elevation", elevation_ft);
    cJSON_AddNumberToObject(data, "reset", reset_reason);

    wireless_publish(DATA_TOPIC, data, 2, false, 5000 / portTICK_PERIOD_MS);

    wireless_stop(5000 / portTICK_PERIOD_MS);

    device_is_setup = true;
  } else {
    ESP_LOGI(TAG, "Waking up sensors");
    for (Sensor *sensor : sensors) {
      err = sensor->ready();
      if (err) {
        ESP_LOGE(TAG, "An error occurred waking up %s.", 
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

    // create json data payload
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

    // wait until wifi is connected
    xTaskNotifyWait(0, -1, reinterpret_cast<uint32_t *>(&err), portMAX_DELAY);
    if (!err) {
      // get information about the wifi signal strength
      int signal_strength;
      err = wireless_get_rssi(&signal_strength);
      if (!err) {
        cJSON_AddNumberToObject(json, SIGNAL_STRENGTH_KEY, signal_strength);
      }

      // publish data to mqtt broker
      wireless_publish(DATA_TOPIC, json, 2, false, 10000 / portTICK_PERIOD_MS);
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
