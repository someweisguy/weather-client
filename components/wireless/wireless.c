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
#include <ctype.h>
#include <string.h>

#define DISCOVER_PREFIX     "homeassistant"
#define STATE_PREFIX        "weather-station"

#define WIFI_DISCONNECTED   BIT(0)
#define WIFI_CONNECTED      BIT(1)
#define WIFI_STOPPED        BIT(2)
#define MQTT_DISCONNECTED   BIT(3)
#define MQTT_CONNECTED      BIT(4)
#define SNTP_SYNCHRONIZED   BIT(5)


static const char *TAG = "wireless";
static esp_netif_t *netif;
static esp_mqtt_client_handle_t mqtt_client;
static EventGroupHandle_t wireless_event_group;
static QueueHandle_t publish_queue;
static char *http_output_buffer;

static void mqtt_handler(void *args, esp_event_base_t base, int event, 
    void *data) {
  if (event == MQTT_EVENT_CONNECTED) {
    ESP_LOGI(TAG, "MQTT connected!");

    // set the connected bit and clear the disconnected bit
    xEventGroupClearBits(wireless_event_group, MQTT_DISCONNECTED);
    xEventGroupSetBits(wireless_event_group, MQTT_CONNECTED);

  } else if (event == MQTT_EVENT_DISCONNECTED) {
    ESP_LOGW(TAG, "MQTT disconnected!");

    // set the disconnected bit and clear the connected bit
    xEventGroupClearBits(wireless_event_group, MQTT_CONNECTED);
    xEventGroupSetBits(wireless_event_group, MQTT_DISCONNECTED);

  } else if (event == MQTT_EVENT_PUBLISHED) {
    esp_mqtt_event_t *event_data = (esp_mqtt_event_t *)data;
    ESP_LOGI(TAG, "MQTT message %i published", event_data->msg_id);

    // send a publish success event to the mqtt queue
    publish_event_t event = { .err = ESP_OK, .msg_id = event_data->msg_id };
    xQueueSendToBack(publish_queue, &event, 10000 / portTICK_PERIOD_MS);

  } else if (event == MQTT_EVENT_ERROR) {
    esp_mqtt_event_t *event_data = (esp_mqtt_event_t *)data;
    ESP_LOGE(TAG, "MQTT message %i failed", event_data->msg_id);

    // send a publish failure event to the mqtt queue
    publish_event_t event = { .err = ESP_FAIL, .msg_id = event_data->msg_id };
    xQueueSendToBack(publish_queue, &event, 10000 / portTICK_PERIOD_MS);
  }
}

static void wifi_handler(void *args, esp_event_base_t base, int event, 
    void *data) {
  if (base == WIFI_EVENT) {
    if (event == WIFI_EVENT_STA_START) {
      ESP_LOGI(TAG, "Connecting to WiFi...");
      xEventGroupClearBits(wireless_event_group, WIFI_STOPPED);
      esp_wifi_connect();
    } else if (event == WIFI_EVENT_STA_DISCONNECTED) {
      wifi_event_sta_disconnected_t *wifi_data = (wifi_event_sta_disconnected_t *)data;
      if (wifi_data->reason != WIFI_REASON_ASSOC_LEAVE) {
        ESP_LOGW(TAG, "WiFi disconnected (reason: %i)", wifi_data->reason);
        ESP_LOGI(TAG, "Attempting to reconnect to WiFi...");
        esp_wifi_connect();
      }
      xEventGroupClearBits(wireless_event_group, WIFI_CONNECTED);
      xEventGroupSetBits(wireless_event_group, WIFI_DISCONNECTED);
    } else if (event == WIFI_EVENT_STA_STOP) {
      ESP_LOGD(TAG, "WiFi stopped");
      xEventGroupSetBits(wireless_event_group, WIFI_STOPPED);
    }
  } else if (event == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(TAG, "WiFi connected");
    xEventGroupClearBits(wireless_event_group, WIFI_DISCONNECTED);
    xEventGroupSetBits(wireless_event_group, WIFI_CONNECTED);

    // check if mqtt should be initialized
    if (mqtt_client == NULL) {
      // create the publish queue and initialize mqtt
      publish_queue = xQueueCreate(10, sizeof(publish_event_t));
      const char *broker = args;
      esp_mqtt_client_config_t mqtt_config = { .host = broker, .keepalive = 45 };
      mqtt_client = esp_mqtt_client_init(&mqtt_config);

      // register event handlers and connect to mqtt
      esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, 
        mqtt_handler, NULL);
      esp_mqtt_client_start(mqtt_client);
    }
  }
}

static void sntp_callback(struct timeval *tv) {
  ESP_LOGI(TAG, "Synchronized time with SNTP server");
  setenv("TZ", "PST8PDT", 1);
  tzset();
  xEventGroupSetBits(wireless_event_group, SNTP_SYNCHRONIZED);
}

static esp_err_t http_event_handler(esp_http_client_event_t *event) {
  if (event->event_id == HTTP_EVENT_ON_DATA) {
    const int content_length = esp_http_client_get_content_length(event->client);
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d content-length=%d", 
      event->data_len, content_length);
    if (http_output_buffer == NULL) {
      if (content_length > 0) http_output_buffer = malloc(content_length + 1);
      else http_output_buffer = malloc(event->data_len + 1);
      http_output_buffer[0] = '\0';
    }
    strncat(http_output_buffer, event->data, event->data_len);
    ESP_LOGD(TAG, "%s", http_output_buffer);
  } else if (event->event_id == HTTP_EVENT_DISCONNECTED) {
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
  } else if (event->event_id == HTTP_EVENT_ERROR) {
    ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
  }

  return ESP_OK;
}

esp_err_t wireless_start(const char *ssid, const char *password, 
    const char *broker) {
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
    ESP_LOGE(TAG, "An error occurred getting the WiFi configuration");
    return err;
  }
  
  // copy wifi credentials to config
  memcpy(wifi_config.sta.ssid, ssid, 32);
  memcpy(wifi_config.sta.password, password, 64);

  // register wifi event handlers
  esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_MASK_ALL, 
    &wifi_handler, NULL, NULL);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
    &wifi_handler, (char *)broker, NULL);

  // set wifi station configuration
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
  esp_wifi_start();

  return ESP_OK;
}

esp_err_t wireless_wait_for_connect(TickType_t timeout) {
  // block until wifi and mqtt connects or fails
  const EventBits_t wireless_status = xEventGroupWaitBits(wireless_event_group, 
    WIFI_CONNECTED | MQTT_CONNECTED, pdFALSE, pdTRUE, timeout);
  if (!(wireless_status & (WIFI_CONNECTED | MQTT_CONNECTED))) {
    if (!(wireless_status & WIFI_CONNECTED)) {
      ESP_LOGE(TAG, "Timed out waiting for WiFi to connect");
    } else {
      ESP_LOGE(TAG, "Timed out waiting for MQTT to connect");
    }
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t wireless_stop(TickType_t timeout) {
  if (wireless_event_group == NULL) return ESP_OK;
  const TickType_t start_tick = xTaskGetTickCount();

  // disconnect and stop mqtt
  ESP_LOGI(TAG, "Stopping MQTT...");
  esp_mqtt_client_destroy(mqtt_client);
  xEventGroupClearBits(wireless_event_group, MQTT_CONNECTED);
  xEventGroupSetBits(wireless_event_group, MQTT_DISCONNECTED);

  // disconnect the wifi client
  ESP_LOGI(TAG, "Stopping WiFi...");
  esp_wifi_disconnect();
  EventBits_t wifi_status = xEventGroupWaitBits(wireless_event_group,
    WIFI_DISCONNECTED, pdFALSE, pdFALSE, timeout);
  if (!(wifi_status & WIFI_DISCONNECTED)) {
    ESP_LOGE(TAG, "Timed out waiting for WiFi to disconnect");
    return ESP_ERR_TIMEOUT;
  }
  timeout -= xTaskGetTickCount() - start_tick;

  // stop the wifi client
  esp_wifi_stop();
  wifi_status = xEventGroupWaitBits(wireless_event_group, WIFI_STOPPED,
    pdFALSE, pdFALSE, timeout);
  if (!(wifi_status & WIFI_STOPPED)) {
    ESP_LOGE(TAG, "Timed out waiting for WiFi to stop");
    return ESP_ERR_TIMEOUT;
  }

  // free resources
  esp_wifi_deinit();
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
    ESP_LOGE(TAG, "Timed out waiting for SNTP to synchronize");
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

esp_err_t wireless_get_location(float *latitude, float *longitude, 
    float *elevation_m) {
  // configure the http client, set lat/long request url
  esp_http_client_config_t config = {
    .url = "http://ipinfo.io/json",
    .event_handler = http_event_handler };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  // send the latitude/longitude request
  esp_err_t err = esp_http_client_perform(client);
  if (err) {
    ESP_LOGE(TAG, "Unable to send latitude/longitude request (0x%x)", err);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
  }

  // read the lat/long response into a buffer
  int status_code = esp_http_client_get_status_code(client);
  if (status_code != 200) ESP_LOGW(TAG, "Latitude/longitude HTTP status: %i", 
    status_code);
  esp_http_client_close(client);

  // parse the lat/long response
  cJSON *root = cJSON_Parse(http_output_buffer);
  free(http_output_buffer);
  http_output_buffer = NULL;
  if (root == NULL || !cJSON_HasObjectItem(root, "loc")) {
    ESP_LOGE(TAG, "Unable to parse latitude/longitude response");
    esp_http_client_cleanup(client);
    return ESP_FAIL;
  }
  cJSON *node = cJSON_GetObjectItem(root, "loc");
  sscanf(node->valuestring, "%f,%f", latitude, longitude);
  cJSON_Delete(root);

  // reconfigure the client for the elevation request
  const char *fmt = "http://nationalmap.gov/epqs/pqs.php?x=%.6f&y=%.6f&units=Meters&output=json";
  char url[strlen(fmt) + (11 * 2)];
  snprintf(url, sizeof(url), fmt, *longitude, *latitude);
  esp_http_client_set_url(client, url);

  // send the elevation request
  err = esp_http_client_perform(client);
  if (err) {
    ESP_LOGE(TAG, "Unable to send elevation request (0x%x)", err);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
  }

  // read the elevation response into a buffer
  status_code = esp_http_client_get_status_code(client);
  if (status_code != 200) ESP_LOGW(TAG, "Elevation HTTP status: %i", 
    status_code);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  // parse the elevation response
  const char* nodes[] = {"USGS_Elevation_Point_Query_Service", "Elevation_Query", "Elevation"};
  root = cJSON_Parse(http_output_buffer);
  free(http_output_buffer);
  http_output_buffer = NULL;
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
  *elevation_m = (float)(node->valuedouble);
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

static int wireless_publish(const char *topic, const cJSON* json, 
    int qos, bool retain) {
  if (mqtt_client == NULL) return -1;

  // publish the message to mqtt
  int msg_id; 
  if (json != NULL) {
    char *payload = cJSON_PrintUnformatted(json);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, qos, retain);
    free(payload);
  } else {
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, NULL, 0, qos, retain);
  }
  
  return msg_id;
}

int wireless_publish_discover(const char *sensor_name, const discovery_t *discovery) {
  if (sensor_name == NULL || discovery == NULL || mqtt_client == NULL)
    return -1;

  // get mac address for device id
  uint64_t mac = 0;
  esp_efuse_mac_get_default((uint8_t *)&mac);

  // create the json discovery object  
  cJSON *json = cJSON_CreateObject();

  // add the required parameters
  cJSON_AddBoolToObject(json, "force_update", discovery->config.force_update);
  cJSON_AddStringToObject(json, "name", discovery->config.name);
  cJSON_AddStringToObject(json, "value_template", discovery->config.value_template);

  // add the optional params
  if (discovery->config.device_class != NULL)
    cJSON_AddStringToObject(json, "device_class", discovery->config.device_class);
  if (discovery->config.icon != NULL)
    cJSON_AddStringToObject(json, "icon", discovery->config.icon);
  if (discovery->config.unit_of_measurement != NULL)
    cJSON_AddStringToObject(json, "unit_of_measurement", 
      discovery->config.unit_of_measurement);

  // add preset params
  cJSON_AddNumberToObject(json, "expire_after", 360);
  cJSON_AddNumberToObject(json, "qos", 2);
  
  // get the state topic
  char state_topic[strlen(STATE_PREFIX) + strlen(sensor_name) + 21];
  snprintf(state_topic, sizeof(state_topic), "%s/%llx/%s/state", 
    STATE_PREFIX, mac, sensor_name);
  cJSON_AddStringToObject(json, "state_topic", state_topic);

  // generate and attach a unique id using mac address
  char unique_id[strlen(discovery->config.name) + 14];
  snprintf(unique_id, sizeof(unique_id), "%s-%llx", discovery->config.name, mac);
  cJSON_AddStringToObject(json, "unique_id", unique_id);

  // get the device id as a string
  char device_id[13];
  snprintf(device_id, sizeof(device_id), "%llx", mac);

  // add device parameters
  cJSON *device = cJSON_CreateObject();
  cJSON_AddStringToObject(device, "manufacturer", "Mitch Weisbrod");
  cJSON_AddStringToObject(device, "sw_version", "");
  cJSON_AddStringToObject(device, "name", "Weather Station");
  cJSON_AddStringToObject(device, "model", "");
  cJSON_AddStringToObject(device, "identifiers", device_id);
  cJSON_AddItemToObject(json, "device", device);

  // get a legal object id to use for the discovery topic
  char legal_name[strlen(discovery->config.name) + 1];
  snprintf(legal_name, sizeof(legal_name), "%s", discovery->config.name);
  for (char *c = legal_name; *c != '\0'; ++c) {
    if (!(isalnum(*c) || *c == '-' || *c == '_')) *c = '_';
  }

  // get the discovery topic
  char discover_topic[strlen(DISCOVER_PREFIX) + strlen(sensor_name) 
    + strlen(discovery->config.name) + 30];
  snprintf(discover_topic, sizeof(discover_topic), "%s/%s/%llx/%s-%s/config",
    DISCOVER_PREFIX, "sensor", mac, sensor_name, legal_name);

  // publish the discovery and free resources
  const int msg_id = wireless_publish(discover_topic, json, 1, true);
  cJSON_Delete(json);

  return msg_id;
}

int wireless_publish_state(const char *sensor_name, cJSON *payload) {
  if (sensor_name == NULL || payload == NULL || mqtt_client == NULL)
    return -1;

  // get mac address for device id
  uint64_t mac = 0;
  esp_efuse_mac_get_default((uint8_t *)&mac);

  // get the state topic
  char state_topic[strlen(STATE_PREFIX) + strlen(sensor_name) + 21];
  snprintf(state_topic, sizeof(state_topic), "%s/%llx/%s/state", 
    STATE_PREFIX, mac, sensor_name);

  // publish the data
  const int msg_id = wireless_publish(state_topic, payload, 2, false);

  return msg_id;
}

esp_err_t wireless_wait_for_publish(publish_event_t *event, TickType_t timeout) { 
  // block until mqtt publishes or times out
  if (xQueueReceive(publish_queue, event, timeout) == pdFALSE) {
    ESP_LOGE(TAG, "MQTT timed out waiting for publish");
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}

int wireless_get_outbox_size() {
  return esp_mqtt_client_get_outbox_size(mqtt_client);
}