#include "cJSON.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include <sys/time.h>

#include "secrets.h"

#include "serial.h"
#include "wireless.h"

#include "sensor.hpp"
#include "bme280.hpp"
#include "pms5003.hpp"
#include "max17043.hpp"
#include "sph0645.hpp"

#define SYSTEM_KEY          "esp32"
#define SIGNAL_STRENGTH_KEY "signal_strength"
#define RESET_REASON_KEY    "reset_reason"

static const char *TAG = "main";
RTC_DATA_ATTR bool device_is_setup;
RTC_DATA_ATTR double latitude, longitude, elevation_m;
RTC_DATA_ATTR time_t last_time_sync_ts;

static Sensor *sensors[] = { new bme280_t(0x76, elevation_m), 
  new pms5003_t(), new max17043_t(0x36), new sph0645_t() };

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
    ESP_LOGE(TAG, "Unable to start serial drivers (%x). Restarting...", 
      err);
    esp_restart();
  }

  if (!device_is_setup) {
    ESP_LOGI(TAG, "Doing initial device setup");

    ESP_LOGI(TAG, "Connecting to wireless services...");
    wireless_start(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER);

    ESP_LOGI(TAG, "Initializing sensors...");
    for (Sensor *sensor : sensors) {
      err = sensor->setup();
      if (err) {
        // TODO: will this boot loop?
        ESP_LOGE(TAG, "An error occurred initializing %s (%x).",
          sensor->get_name(), err);
        //esp_restart();
      }
    }

    // wait for wifi to connect
    err = wireless_wait_for_connect(15000 / portTICK_PERIOD_MS);
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
          .force_update = true,
          .icon = nullptr,
          .name = "Signal Strength",
          .unit_of_measurement = "dB",
          .value_template = "{{ value_json." SIGNAL_STRENGTH_KEY " }}"
        },
      }
    };

    // send default discovery
    ESP_LOGI(TAG, "Sending MQTT discoveries...");
    for (discovery_t discovery : default_discoveries) {
      wireless_publish_discover(SYSTEM_KEY, &discovery);
      err = wireless_wait_for_publish(nullptr, 30000 / portTICK_PERIOD_MS);
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
        wireless_publish_discover(sensor->get_name(), &discoveries[i]);
        err = wireless_wait_for_publish(nullptr, 30000 / portTICK_PERIOD_MS);
        if (err) {
          ESP_LOGE(TAG, "An error occurred sending discovery. Restarting...");
          esp_restart();
        }
      }
    }

    // build system data json object
    // cJSON *json = cJSON_CreateObject()
    //cJSON_AddNumberToObject(system, RESET_REASON_KEY, esp_reset_reason());
    //cJSON_AddItemToObject(json, SYSTEM_KEY, system);

    // get wifi signal strength
    ESP_LOGI(TAG, "Publishing signal strength...");
    int signal_strength;
    err = wireless_get_rssi(&signal_strength);
    if (!err) {
      cJSON *wireless = cJSON_CreateObject();
      cJSON_AddNumberToObject(wireless, SIGNAL_STRENGTH_KEY, signal_strength);
      wireless_publish_state(SYSTEM_KEY, wireless, nullptr);
      cJSON_Delete(wireless);

      // publish the setup data
      err = wireless_wait_for_publish(nullptr, 10000 / portTICK_PERIOD_MS);
      if (err) {
        ESP_LOGE(TAG, "An error occurred sending setup data.");
      }
    }

    // stop wireless radios and setup is finished
    wireless_stop(5000 / portTICK_PERIOD_MS);
    device_is_setup = true;

  } else {
    ESP_LOGI(TAG, "Readying sensors");
    for (Sensor *sensor : sensors) {
      err = sensor->ready();
      if (err) {
        ESP_LOGE(TAG, "An error occurred readying the %s (%x).", 
          sensor->get_name(), err);
      }
    }

    // wait until 15s before measurement to start wifi and mqtt
    const int wifi_time_ms = 15 * 1000;
    int wait_time_ms = (time_to_next_state_us() / 1000) - wifi_time_ms;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);

    // connect to wifi and mqtt
    wireless_start(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER);

    // create json json payload
    const int num_sensors = sizeof(sensors) / sizeof(Sensor *);
    esp_err_t sensor_returns[num_sensors] = {};
    int msg_ids[num_sensors + 1];
    cJSON *payloads[num_sensors];
    for (int i = 0; i < num_sensors; ++i) payloads[i] = cJSON_CreateObject();
    bool need_to_restart = false;
    int num_publishes = 0;

    // wait until measurement window
    wait_time_ms = time_to_next_state_us() / 1000;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);

    // get the datetime - for debugging
    char datetime[77];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(datetime, 76, "%d-%02d-%02d %02d:%02d:%02d UTC", tm.tm_year + 1900,
      tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    // get sensor data - increment num_payloads on success
    ESP_LOGI(TAG, "Getting sensor data...");
    for (int i = 0; i < num_sensors; ++i) {
      err = sensors[i]->get_data(payloads[i]);
      if (err) {
        ESP_LOGE(TAG, "An error occurred getting data from %s (%x).", 
          sensors[i]->get_name(), err);
        sensor_returns[i] = err;
        need_to_restart = true;
      }
    }

    // publish json to mqtt broker - will queue publishes if not connected
    for (int i = 0; i < num_sensors; ++i) {
      if (sensor_returns[i] == ESP_OK) {
        wireless_publish_state(sensors[i]->get_name(), payloads[i], &msg_ids[i]);
        ++num_publishes;
      } else {
        msg_ids[i] = -1;
      }
    }

    // put sensors to sleep
    for (Sensor *sensor : sensors) {
      err = sensor->sleep();
      if (err) { 
        ESP_LOGE(TAG, "An error occurred putting the %s to sleep (%x).", 
          sensor->get_name(), err);
        need_to_restart = true;
      }
    }

    // wait until wifi is connected
    err = wireless_wait_for_connect(15000 / portTICK_PERIOD_MS);
    if (!err) {
      // get information about the wifi signal strength
      int signal_strength;
      err = wireless_get_rssi(&signal_strength);
      if (!err) {
        cJSON *wireless = cJSON_CreateObject();
        cJSON_AddNumberToObject(wireless, SIGNAL_STRENGTH_KEY, signal_strength);
        
        // add datetime string for debugging
        cJSON_AddStringToObject(wireless, "now", datetime);

        wireless_publish_state(SYSTEM_KEY, wireless, &msg_ids[num_sensors]);
        ++num_publishes;
        cJSON_Delete(wireless);
      }

      // wait for each message to finish getting published
      TickType_t timeout = 50000 / portTICK_PERIOD_MS;
      while (num_publishes--) {
        int msg_id;
        TickType_t start_tick = xTaskGetTickCount();
        err = wireless_wait_for_publish(&msg_id, timeout);
        if (err == ESP_ERR_TIMEOUT) {
          ESP_LOGE(TAG, "%i publish(es) timed out.", num_publishes);
          need_to_restart = true;
          break;
        } else if (err) {
          // there was a error sending the message, but it didn't time out
          for (int i = 0; i < num_sensors; ++i) {
            if (msg_id == msg_ids[i]) {
              ESP_LOGW(TAG, "There was an error delivering the state for %s. Re-publishing...",
                sensors[i]->get_name());
              wireless_publish_state(sensors[i]->get_name(), payloads[i],
                &msg_ids[i]);
              break;
            }
          }
        }
        timeout -= xTaskGetTickCount() - start_tick;
      }

      wireless_stop(10000 / portTICK_PERIOD_MS);
    }

    // delete the json payloads
    for (cJSON *payload : payloads) {
      cJSON_Delete(payload);
    }


    if (need_to_restart) {
      ESP_LOGW(TAG, "Restarting...");
      esp_restart();
    }

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
