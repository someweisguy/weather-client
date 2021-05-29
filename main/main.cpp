#include "driver/gpio.h"
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

#define SYSTEM_KEY          "esp32"
#define SIGNAL_STRENGTH_KEY "signal_strength"

static const char *TAG = "main";
RTC_DATA_ATTR bool device_is_setup;
RTC_DATA_ATTR float latitude, longitude, elevation_m;
RTC_DATA_ATTR time_t last_time_sync_ts;

static sensor_t *sensors[] = { new bme280_t(0x76, &elevation_m), 
  new pms5003_t(GPIO_NUM_14), new max17043_t(0x36), new sph0645_t() };

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
    ESP_LOGE(TAG, "Unable to start serial drivers (0x%x)", err);
    esp_restart();
  }

  if (!device_is_setup) {
    // do initial device setup
    wireless_start(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER);

    ESP_LOGI(TAG, "Initializing sensors...");
    for (sensor_t *sensor : sensors) {
      err = sensor->setup();
      if (err) {
        ESP_LOGE(TAG, "An error occurred initializing %s (0x%x).",
          sensor->get_name(), err);
        //esp_restart();
      }
    }

    // wait for wifi to connect
    err = wireless_wait_for_connect(15000 / portTICK_PERIOD_MS);
    if (err) {
      ESP_LOGE(TAG, "Restarting...");
      esp_restart();
    }

    // synchronize time with sntp server
    ESP_LOGD(TAG, "Synchronizing time...");
    err = wireless_synchronize_time(SNTP_SERVER, 15000 / portTICK_PERIOD_MS);
    if (err) {
      ESP_LOGE(TAG, "Restarting...");
      esp_restart();
    }
    time(&last_time_sync_ts); // log the timestamp that time was sync'd

    ESP_LOGI(TAG, "Getting location...");
    err = wireless_get_location(&latitude, &longitude, &elevation_m);
    if (err) {
      ESP_LOGE(TAG, "Unable to get location. Restarting...");
      esp_restart();
    }

    // declare default discovery for device
    const discovery_t signal_strength_discovery = {
      .topic = "sensor/signal_strength",
        .config = {
          .device_class = "signal_strength",
          .force_update = true,
          .icon = nullptr,
          .name = "Signal Strength",
          .unit_of_measurement = "dB",
          .value_template = "{{ value_json." SIGNAL_STRENGTH_KEY " }}"
        }
    };

    // send discovery messages
    ESP_LOGI(TAG, "Sending MQTT discoveries...");
    wireless_publish_discover(SYSTEM_KEY, &signal_strength_discovery);
    publish_event_t event;
    err = wireless_wait_for_publish(&event, 30000 / portTICK_PERIOD_MS);
    if (err || event.err) {
      ESP_LOGE(TAG, "An error occurred sending discovery. Restarting...");
      esp_restart();
    }
    for (sensor_t *sensor : sensors) {
      // publish each discovery for every sensor
      const discovery_t *discoveries;
      const int num_discoveries = sensor->get_discovery(discoveries);
      for (int i = 0; i < num_discoveries; ++i) { 
        wireless_publish_discover(sensor->get_name(), &discoveries[i]);
        err = wireless_wait_for_publish(&event, 10000 / portTICK_PERIOD_MS);
        if (err || event.err) {
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
      cJSON *payload = cJSON_CreateObject();
      cJSON_AddNumberToObject(payload, SIGNAL_STRENGTH_KEY, signal_strength);
      wireless_publish_state(SYSTEM_KEY, payload);
      cJSON_Delete(payload);

      // publish the setup data
      err = wireless_wait_for_publish(&event, 60000 / portTICK_PERIOD_MS);
      if (err || event.err) {
        ESP_LOGE(TAG, "An error occurred sending setup data. Restarting...");
        esp_restart();
      }
    }

    // stop wireless radios and setup is finished
    wireless_stop(10000 / portTICK_PERIOD_MS);
    device_is_setup = true;

  } else {
    // wake up and get ready to publish data
    bool error_occurred = false;
    TickType_t timeout = 60000 / portTICK_PERIOD_MS - 1;

    // create sensor data array
    const int num_sensors = sizeof(sensors) / sizeof(sensor_t *);
    const int num_data = num_sensors + 1; // include wifi rssi
    sensor_data_t data[num_data] = {};
    for (int i = 0; i < num_data; ++i) {
      data[i].payload = cJSON_CreateObject();
      if (i < num_sensors) data[i].name = sensors[i]->get_name();
      else data[i].name = SYSTEM_KEY;
    }

    ESP_LOGI(TAG, "Readying sensors...");
    for (int i = 0; i < num_sensors; ++i) {
      err = sensors[i]->ready();
      if (err) {
        ESP_LOGE(TAG, "An error occurred readying %s (0x%x)", 
          sensors[i]->get_name(), err);
        error_occurred = true;
        data[i].err = err;
      }
    }

    // wait until 15s before measurement to start wifi and mqtt
    const int wifi_time_ms = 15 * 1000;
    int wait_time_ms = (time_to_next_state_us() / 1000) - wifi_time_ms;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS);
    wireless_start(WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER);

    // wait until measurement window
    /* wait one extra tick in order to err on the side of the 5 minute mark 
    rather than the 4 minute and 59 second mark */
    wait_time_ms = time_to_next_state_us() / 1000;
    vTaskDelay(wait_time_ms / portTICK_PERIOD_MS + 1);
    TickType_t start_tick = xTaskGetTickCount();

    // get sensor data
    ESP_LOGI(TAG, "Getting sensor data");
    for (int i = 0; i < num_sensors; ++i) {
      if (data[i].err) continue;
      err = sensors[i]->get_data(data[i].payload);
      if (err) {
        ESP_LOGE(TAG, "An error occurred getting data from the %s (0x%x)", 
          sensors[i]->get_name(), err);
        data[i].err = err;
        error_occurred = true;
      } 
    }

    // put sensors to sleep
    for (sensor_t *sensor : sensors) {
      err = sensor->sleep();
      if (err) { 
        ESP_LOGE(TAG, "An error occurred putting the %s to sleep (0x%x)", 
          sensor->get_name(), err);
        error_occurred = true;
      }
    }

    // publish sensor data to mqtt broker
    for (int i = 0; i < num_sensors; ++i) {
      if (!data[i].err) {
        int retries = 5;
        do {
          data[i].msg_id = wireless_publish_state(data[i].name, 
            data[i].payload);
        } while (data[i].msg_id <= 0 && retries--);
      }
    } 

    // wait until wifi is connected
    err = wireless_wait_for_connect(timeout);
    if (err) {
      ESP_LOGE(TAG, "Restarting...");
      esp_restart();
    }  

    // get information about the wifi signal strength
    int signal_strength;
    err = wireless_get_rssi(&signal_strength);
    if (!err) {
      cJSON_AddNumberToObject(data[num_sensors].payload, SIGNAL_STRENGTH_KEY, 
        signal_strength);
      data[num_sensors].msg_id = wireless_publish_state(SYSTEM_KEY, 
        data[num_sensors].payload);
    }

    const int starting_outbox_size = wireless_get_outbox_size();

    // wait for each message to publish
    while (wireless_get_outbox_size()) {
      const TickType_t now_tick = xTaskGetTickCount();
      timeout -= now_tick - start_tick;
      start_tick = now_tick;

      // wait for next publish event
      publish_event_t event;
      err = wireless_wait_for_publish(&event, timeout);
      if (err == ESP_ERR_TIMEOUT) {
        const int ending_outbox_size = wireless_get_outbox_size();
        ESP_LOGE(TAG, "One or more payloads were not delivered (starting outbox: %i, ending: %i)", 
          starting_outbox_size, ending_outbox_size);
        error_occurred = true;
        break;
      }

      // no need to process data if msg_id is invalid
      if (event.msg_id <= 0) continue;

      // determine which message was received on the event
      for (int i = 0; i < num_data; ++i) {
        if (event.msg_id == data[i].msg_id) {
          if (event.err == ESP_FAIL) {
            // message failed to publish
            ESP_LOGW(TAG, "Republishing %s payload...", data[i].name);
            data[i].msg_id = wireless_publish_state(data[i].name, 
              data[i].payload);
          }
          break;
        }
      }

    }
    for (sensor_data_t &datum : data) cJSON_Delete(datum.payload);

    // check if the clock needs to be resynchronized
    const int time_sync_period_s = 89400;
    if (time(nullptr) > last_time_sync_ts + time_sync_period_s) {
      ESP_LOGI(TAG, "Re-synchronizing time...");
      err = wireless_synchronize_time(SNTP_SERVER, 15000 / portTICK_PERIOD_MS);
      if (err) {
        ESP_LOGE(TAG, "Restarting...");
        esp_restart();
      }
      time(&last_time_sync_ts);
    }

    wireless_stop(10000 / portTICK_PERIOD_MS);

    // check if an error occurred between reading sensors and publishing data
    if (error_occurred) {
      ESP_LOGW(TAG, "Restarting...");
      esp_restart();
    }
  }

  ESP_LOGI(TAG, "Going to sleep...");
  const int wake_early_us = 32 * 1e6; 
  int sleep_time_us = time_to_next_state_us() - wake_early_us;
  if (sleep_time_us < 10000) sleep_time_us += 5 * 60 * 1e6; // skip

  esp_deep_sleep(sleep_time_us);
}
