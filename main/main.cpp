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

typedef struct {
  const char *name;
  esp_err_t err;
  int msg_id;
  cJSON *payload;
} sensor_data_t;

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
      publish_event_t event;
      err = wireless_wait_for_publish(&event, 30000 / portTICK_PERIOD_MS);
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
        publish_event_t event;
        wireless_publish_discover(sensor->get_name(), &discoveries[i]);
        err = wireless_wait_for_publish(&event, 30000 / portTICK_PERIOD_MS);
        if (err || event.ret) {
          ESP_LOGE(TAG, "An error occurred sending discovery. Restarting...");
          esp_restart();
        }
      }
    }

    // get wifi signal strength
    ESP_LOGI(TAG, "Publishing signal strength...");
    int signal_strength;
    err = wireless_get_rssi(&signal_strength);
    if (!err) {
      cJSON *wireless = cJSON_CreateObject();
      cJSON_AddNumberToObject(wireless, SIGNAL_STRENGTH_KEY, signal_strength);
      wireless_publish_state(SYSTEM_KEY, wireless);
      cJSON_Delete(wireless);

      // publish the setup data
      publish_event_t event;
      err = wireless_wait_for_publish(&event, 10000 / portTICK_PERIOD_MS);
      if (err || event.ret) {
        ESP_LOGE(TAG, "An error occurred sending setup data.");
      }
    }

    // stop wireless radios and setup is finished
    wireless_stop(5000 / portTICK_PERIOD_MS);
    device_is_setup = true;

  } else {
    // track if an error occurred and number of publishes
    bool error_occurred = false;
    int num_publishes = 0;

    ESP_LOGI(TAG, "Readying sensors");
    for (Sensor *sensor : sensors) {
      err = sensor->ready();
      if (err) {
        ESP_LOGE(TAG, "An error occurred readying %s (%x).", 
          sensor->get_name(), err);
        error_occurred = true;
      }
    }

    // wait until 15s before measurement to start wifi and mqtt
    const int wifi_time_ms = 15 * 1000;
    int wait_time_ms = (time_to_next_state_us() / 1000) - wifi_time_ms;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);

    // connect to wifi and mqtt
    wireless_start(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER);

    // create sensor data array
    const int num_sensors = sizeof(sensors) / sizeof(Sensor *);
    sensor_data_t data[num_sensors + 1] = {};
    for (int i = 0; i <= num_sensors; ++i) {
      data[i].payload = cJSON_CreateObject();
      if (i < num_sensors) data[i].name = sensors[i]->get_name();
      else data[i].name = SYSTEM_KEY;
    }

    // wait until measurement window
    wait_time_ms = time_to_next_state_us() / 1000;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);
    const TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout = 60000 / portTICK_PERIOD_MS;

    // get sensor data
    ESP_LOGI(TAG, "Getting sensor data...");
    for (int i = 0; i < num_sensors; ++i) {
      err = sensors[i]->get_data(data[i].payload);
      if (err) {
        ESP_LOGE(TAG, "An error occurred getting data from %s (%x).", 
          sensors[i]->get_name(), err);
        data[i].err = err;
        error_occurred = true;
      } 
    }

    // put sensors to sleep
    for (Sensor *sensor : sensors) {
      err = sensor->sleep();
      if (err) { 
        ESP_LOGE(TAG, "An error occurred putting the %s to sleep (%x).", 
          sensor->get_name(), err);
        error_occurred = true;
      }
    }

    // publish data to mqtt broker
    for (int i = 0; i < num_sensors; ++i) {
      if (!data[i].err) {
        int retries = 5; // don't loop forever
        do {
          // mqtt messages will queue if not connected to broker
          data[i].msg_id = wireless_publish_state(data[i].name, 
            data[i].payload);
        } while (data[i].msg_id <= 0 && retries--);
        ++num_publishes;
      }
    } 

    // wait until wifi is connected
    err = wireless_wait_for_connect(timeout);
    if (!err) {
      timeout -= xTaskGetTickCount() - start_tick;

      // get information about the wifi signal strength
      int signal_strength;
      err = wireless_get_rssi(&signal_strength);
      if (!err) {
        cJSON_AddNumberToObject(data[num_sensors].payload, 
          SIGNAL_STRENGTH_KEY, signal_strength);
        data[num_sensors].msg_id = wireless_publish_state(SYSTEM_KEY,
          data[num_sensors].payload);
        ++num_publishes;
      }

      // wait for each message to publish
      while (num_publishes) {
        timeout -= xTaskGetTickCount() - start_tick;
        publish_event_t event;
        err = wireless_wait_for_publish(&event, timeout);
        if (!err)  {
          // check if mqtt disconnected, message success, or message failure
          if (event.ret == ESP_OK) {
            // message published successfully
            bool updated_outbox = false;
            for (sensor_data_t &datum : data) {
              if (event.msg_id == datum.msg_id) {
                datum.msg_id = -1;
                updated_outbox = true;
                break;
              }
            }
            if (!updated_outbox) ESP_LOGW(TAG, "Couldn't update MQTT outbox!");
            --num_publishes; // publish success
          } else if (event.ret == ESP_FAIL) {
            // message failed to publish so it should be resent
            for (sensor_data_t &datum : data) {
              if (event.msg_id == datum.msg_id) {
                ESP_LOGW(TAG, "Republishing %s payload...", datum.name);
                datum.msg_id = wireless_publish_state(datum.name, 
                  datum.payload);
                break;
              }
            }
          } else if (event.ret == ESP_ERR_INVALID_STATE) {
            // mqtt disconnected, so all unsent messages should be resent
            err = wireless_wait_for_connect(timeout);
            if (err) {
              ESP_LOGE(TAG, "%i publish(es) timed out.", num_publishes);
              error_occurred = true;
              break;
            }
            int num_republishes = 0;
            for (sensor_data_t &datum : data) {
              if (datum.msg_id > 0) {
                // message hasn't been delivered so republish it
                datum.msg_id = wireless_publish_state(datum.name, 
                  datum.payload);
                ++num_republishes;
              }
            }
            ESP_LOGW(TAG, "Republished %i payloads.", num_republishes);
          }
        } else if (err == ESP_ERR_TIMEOUT) {
          // timed out waiting for messages to send
          ESP_LOGE(TAG, "%i publish(es) timed out.", num_publishes);
          error_occurred = true;
          break;
        }
      }

      wireless_stop(10000 / portTICK_PERIOD_MS);
    }

    // delete the json payloads
    for (sensor_data_t &datum : data) {
      cJSON_Delete(datum.payload);
    }

    // check if an error occurred between reading sensors and publishing data
    if (error_occurred) {
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
