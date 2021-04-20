#include "wireless.h"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <string.h>

#define WIFI_DISCONNECTED   BIT(0)
#define WIFI_CONNECTED      BIT(1)
#define WIFI_FATAL_ERROR    BIT(2)
#define MQTT_DISCONNECTED   BIT(3)
#define MQTT_CONNECTED      BIT(4)
#define MQTT_FATAL_ERROR    BIT(5)
#define MQTT_MSG_PUBLISHED  BIT(6)
#define SNTP_SYNCHRONIZED   BIT(7)

static const char *TAG = "wireless";
static esp_netif_t *netif;
static esp_mqtt_client_handle_t mqtt_client;
static EventGroupHandle_t wireless_event_group;

static void wifi_handler(void *args, esp_event_base_t base, int event, 
    void *data) {
  if (base == WIFI_EVENT) {
    if (event == WIFI_EVENT_STA_START) {
      ESP_LOGI(TAG, "Connecting to WiFi...");
      esp_wifi_connect();
    } else if (event == WIFI_EVENT_STA_DISCONNECTED) {
      wifi_event_sta_disconnected_t *wifi_data = (wifi_event_sta_disconnected_t *)data;
      if (wifi_data->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
        // wifi password was rejected
        ESP_LOGE(TAG, "WiFi password is invalid");
        esp_wifi_stop();
        xEventGroupSetBits(wireless_event_group, WIFI_FATAL_ERROR);
      } else if (wifi_data->reason == WIFI_REASON_ASSOC_LEAVE) {
        ESP_LOGI(TAG, "WiFi stopped!");
        esp_wifi_stop();
      } else {
        ESP_LOGW(TAG, "WiFi disconnected (reason: %i)", wifi_data->reason);
        ESP_LOGI(TAG, "Attempting to reconnect to WiFi...");
        esp_wifi_connect();
      }

      // signal wifi disconnection
      xEventGroupClearBits(wireless_event_group, WIFI_CONNECTED);
      xEventGroupSetBits(wireless_event_group, WIFI_DISCONNECTED);
    }
  } else if (event == IP_EVENT_STA_GOT_IP) {
    // signal wifi connection
    ESP_LOGI(TAG, "WiFi connected!");
    xEventGroupClearBits(wireless_event_group, WIFI_DISCONNECTED);
    xEventGroupSetBits(wireless_event_group, WIFI_CONNECTED);
  }
}

static void mqtt_handler(void *args, esp_event_base_t base, int event, 
    void *data) {
  if (event == MQTT_EVENT_CONNECTED) {
    ESP_LOGI(TAG, "MQTT connected!");

    // set the connected bit and clear the disconnected bit
    xEventGroupClearBits(wireless_event_group, MQTT_DISCONNECTED);
    xEventGroupSetBits(wireless_event_group, MQTT_CONNECTED);

  } else if (event == MQTT_EVENT_DISCONNECTED) {
    ESP_LOGI(TAG, "MQTT disconnected!");

    // set the disconnected bit and clear the connected bit
    xEventGroupClearBits(wireless_event_group, MQTT_CONNECTED);
    xEventGroupSetBits(wireless_event_group, MQTT_DISCONNECTED);

  } else if (event == MQTT_EVENT_PUBLISHED) {
    ESP_LOGI(TAG, "MQTT message published!");

    xEventGroupSetBits(wireless_event_group, MQTT_MSG_PUBLISHED);
  }
}

static void sntp_callback(struct timeval *tv) {
  ESP_LOGI(TAG, "Synchronized time with SNTP server!");
  xEventGroupSetBits(wireless_event_group, SNTP_SYNCHRONIZED);
}

esp_err_t wireless_start(const char *ssid, const char *password, const char *broker, 
    TickType_t timeout) {
  esp_err_t err;

  if (wireless_event_group == NULL) {
    // init event group
    wireless_event_group = xEventGroupCreate();
    
    // init non-volatile flash
    nvs_flash_init();
    
    // init network interface
    err = esp_netif_init();
    if (err) return err;
    esp_event_loop_create_default();
    netif = esp_netif_create_default_wifi_sta();
  }

  // configure wifi initialization
  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&init_config);

  // try to get wifi station configuration
  wifi_config_t wifi_config = {}; // init to null
  err = esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
  if (err) {
    // handle error
    ESP_LOGE(TAG, "An error occurred getting the WiFi credentials");
    return err;
  } else if (strcmp((char *)wifi_config.sta.ssid, ssid) == 0 && 
        strcmp((char *)wifi_config.sta.password, password) == 0) {
    // found wifi credentials in nvs
    ESP_LOGI(TAG, "Found credentials for SSID \"%s\" in non-volatile storage", 
      wifi_config.sta.ssid);
  }
  else
  {
    // no wifi credentials found
    ESP_LOGI(TAG, "Copying new WiFi credentials to driver");
    memcpy(wifi_config.sta.ssid, ssid, 32);
    memcpy(wifi_config.sta.password, password, 64);
  }

  // register wifi event handlers
  esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_MASK_ALL, 
    &wifi_handler, NULL, NULL);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
    &wifi_handler, NULL, NULL);

  // set wifi station configuration
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
  esp_wifi_start();

  // block until wifi connects or fails
  const TickType_t start_tick = xTaskGetTickCount();
  const EventBits_t wifi_status = xEventGroupWaitBits(wireless_event_group, 
    WIFI_CONNECTED | WIFI_FATAL_ERROR, pdFALSE, pdFALSE, timeout);
  timeout -= xTaskGetTickCount() - start_tick;
  if (wifi_status & WIFI_FATAL_ERROR) {
    return ESP_ERR_INVALID_ARG;
  } else if (!(wifi_status & WIFI_CONNECTED)) {
    // timed out waiting for wifi to connect
    ESP_LOGE(TAG, "WiFi timed out");
    return ESP_ERR_TIMEOUT;
  }
  
  // configure and initialize mqtt
  esp_mqtt_client_config_t mqtt_config = {.host = broker};
  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  // register mqtt event handlers
  esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, 
    mqtt_handler, NULL);

  // connect to mqtt
  esp_mqtt_client_start(mqtt_client);

  // block until mqtt connects or fails
  const EventBits_t mqtt_status = xEventGroupWaitBits(wireless_event_group, 
    MQTT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
  if (!(mqtt_status & MQTT_CONNECTED)) {
    // timed out waiting for mqtt to connect
    ESP_LOGE(TAG, "MQTT timed out");
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t wireless_stop(TickType_t timeout) {
  if (wireless_event_group == NULL)
    return ESP_OK;

  // disconnect and stop mqtt
  ESP_LOGI(TAG, "Stopping MQTT...");
  esp_mqtt_client_disconnect(mqtt_client);
  const TickType_t start_tick = xTaskGetTickCount();
  const EventBits_t mqtt_status = xEventGroupWaitBits(wireless_event_group,
    MQTT_DISCONNECTED, pdFALSE, pdFALSE, timeout);
  if (!(mqtt_status & MQTT_DISCONNECTED)) {
    // timed out waiting for mqtt to disconnect
    ESP_LOGE(TAG, "MQTT timed out");
    return ESP_ERR_TIMEOUT;
  }
  timeout -= start_tick - xTaskGetTickCount();
  esp_mqtt_client_stop(mqtt_client);

  // disconnect and stop wifi
  ESP_LOGI(TAG, "Stopping WiFi...");
  esp_wifi_disconnect();
  const EventBits_t wifi_status = xEventGroupWaitBits(wireless_event_group,
    WIFI_DISCONNECTED, pdFALSE, pdFALSE, timeout);
  if (!(wifi_status & WIFI_DISCONNECTED)) {
    // timed out waiting for wifi to disconnect
    ESP_LOGE(TAG, "WiFi timed out");
    return ESP_ERR_TIMEOUT;
  }

  netif = NULL;

  return ESP_OK;
}

esp_err_t wireless_synchronize_time(const char *server, TickType_t timeout) {
  // setup and initialize the sntp client
  sntp_set_time_sync_notification_cb(sntp_callback);
  sntp_setoperatingmode(SNTP_SYNC_MODE_IMMED);
  sntp_setservername(0, server);
  sntp_init();

  // block until sntp connects or times out
  const EventBits_t sntp_status = xEventGroupWaitBits(wireless_event_group,
    SNTP_SYNCHRONIZED, pdTRUE, pdFALSE, timeout);
  if (!(sntp_status & SNTP_SYNCHRONIZED)) {
    // timed out waiting for sntp to disconnect
    ESP_LOGE(TAG, "SNTP timed out");
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t wireless_get_location(double *latitude, double *longitude, 
    double *elevation) {
  // declare http buffer parameters
  const int buffer_size = 512;
  int response_len, status_code;
  char response_buffer[buffer_size];

  // configure the http client, set lat/long request url
  esp_http_client_config_t config = {.buffer_size = 1024, .url = "http://ipinfo.io/json"};
  esp_http_client_handle_t client = esp_http_client_init(&config);

  // send the latitude/longitude request
  esp_err_t err = esp_http_client_perform(client);
  if (err) {
    ESP_LOGE(TAG, "Unable to send latitude/longitude request");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
  }

  // read the lat/long response into a buffer
  status_code = esp_http_client_get_status_code(client);
  if (status_code != 200) {
    ESP_LOGW(TAG, "Latitude/longitude response status code: %i", status_code);
  }
  response_len = esp_http_client_read(client, response_buffer, buffer_size);
  esp_http_client_close(client);

  // parse the lat/long response
  cJSON *root = cJSON_Parse(response_buffer);
  if (root == NULL || !cJSON_HasObjectItem(root, "loc")) {
    ESP_LOGE(TAG, "Unable to parse latitude/longitude response");
    esp_http_client_cleanup(client);
    return ESP_FAIL;
  }
  cJSON *node = cJSON_GetObjectItem(root, "loc");
  sscanf(node->valuestring, "%lf,%lf", latitude, longitude);
  cJSON_Delete(root);

  // reconfigure the client for the elevation request
  const char *fmt = "http://nationalmap.gov/epqs/pqs.php?x=%.6lf&y=%.6lf&units=Meters&output=json";
  char url[strlen(fmt) + (11 * 2)];
  snprintf(url, sizeof(url), fmt, *longitude, *latitude);
  esp_http_client_set_url(client, url);

  // send the elevation request
  err = esp_http_client_perform(client);
  if (err) {
    ESP_LOGE(TAG, "Unable to send elevation request");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
  }

  // read the elevation response into a buffer
  status_code = esp_http_client_get_status_code(client);
  if (status_code != 200) {
    ESP_LOGW(TAG, "Elevation response status code: %i", status_code);
  }
  response_len = esp_http_client_read(client, response_buffer, buffer_size);
  response_buffer[response_len + 1] = 0; // null terminate
  esp_http_client_close(client);
  esp_http_client_cleanup(client); // cleanup resources

  // parse the elevation response
  const char* nodes[] = {"USGS_Elevation_Point_Query_Service", "Elevation_Query", "Elevation"};
  root = cJSON_Parse(response_buffer);
  if (root == NULL) {
    ESP_LOGE(TAG, "Unable to parse elevation response");
    return ESP_FAIL;
  }
  node = cJSON_GetObjectItem(root, nodes[0]);
  for (int i = 1; i < sizeof(nodes) / sizeof(char*); ++i) {
    if (node == NULL) {
      ESP_LOGE(TAG, "Unable to parse elevation response");
      return ESP_FAIL;
    }
    node = cJSON_GetObjectItem(node, nodes[i]);
  }
  *elevation = node->valuedouble;
  cJSON_Delete(root);

  return ESP_OK;
}

esp_err_t wireless_get_rssi(int *rssi) {
  // get the wireless antenna rssi
  wifi_ap_record_t ap_info;
  esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
  if (!err) {
    *rssi = ap_info.rssi;
  } else {
    ESP_LOGE(TAG, "Unable to get RSSI");
    return err;
  }
  
  return ESP_OK;
}

esp_err_t wireless_publish(const char *topic, cJSON* json, int qos, 
    bool retain, TickType_t timeout) {
  if (mqtt_client == NULL) return ESP_ERR_INVALID_STATE;

  // publish the message to mqtt
  if (json != NULL) {
    char *payload = cJSON_PrintUnformatted(json);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, qos, retain);
    free(payload);
  } else {
    esp_mqtt_client_publish(mqtt_client, topic, NULL, 0, qos, retain);
  }

  // block until mqtt publishes or times out
  const EventBits_t mqtt_status = xEventGroupWaitBits(wireless_event_group,
    MQTT_MSG_PUBLISHED, pdTRUE, pdFALSE, timeout);
  if (!(mqtt_status & MQTT_MSG_PUBLISHED)) {
    // timed out waiting for mqtt to publish
    ESP_LOGE(TAG, "MQTT timed out");
    return ESP_ERR_TIMEOUT;
  }
  
  return ESP_OK;
}

esp_err_t wireless_discover(const discovery_t *discovery, int qos, bool retain,
    TickType_t timeout) {
  if (discovery == NULL) return ESP_ERR_INVALID_ARG;  

  // create the json object and fill it with required params
  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "expire_after", discovery->config.expire_after);
  cJSON_AddBoolToObject(json, "force_update", discovery->config.force_update);
  cJSON_AddStringToObject(json, "icon", discovery->config.icon);
  cJSON_AddStringToObject(json, "name", discovery->config.name);
  cJSON_AddNumberToObject(json, "qos", discovery->config.qos);
  cJSON_AddStringToObject(json, "state_topic", discovery->config.state_topic);
  cJSON_AddStringToObject(json, "unique_id", discovery->config.unique_id);
  cJSON_AddStringToObject(json, "unit_of_measurement", discovery->config.unit_of_measurement);
  cJSON_AddStringToObject(json, "value_template", discovery->config.value_template);

  // handle the optional params
  if (discovery->device_class != NULL) {
    cJSON_AddStringToObject(json, "device_class", discovery->device_class);
  }

  // TODO: add device, state_topic, etc.

  // publish the discovery
  esp_err_t err = wireless_publish(discovery->topic, json, qos, retain, 
    timeout);
  cJSON_Delete(json);

  return err;
}
