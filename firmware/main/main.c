#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "hal/gpio_types.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "soc/gpio_num.h"
#include <stdint.h>

static const char *TAG = "main";
static esp_mqtt_client_handle_t mqtt_client_handle;

static char device_id[32];
static char mac_str[32];
static char ip_str[16];

#define MY_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define MY_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD

void get_mac_str(char *mac_str, size_t size) {
  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
  snprintf(mac_str, size, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
}

void get_device_id(char *device_id, size_t size) {
  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

  snprintf(device_id, size, "esp32-%02X%02X%02X%02X%02X%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
}

void create_status_json(char *status_str, int status) {
  cJSON *status_root = cJSON_CreateObject();
  cJSON_AddNumberToObject(status_root, "status", status);
  char *json = cJSON_PrintUnformatted(status_root);
  strcpy(status_str, json);

  free(json);
  cJSON_Delete(status_root);
}

void mqtt_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {

  esp_mqtt_event_handle_t event = event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED: {
    ESP_LOGI("MQTT", "Connected to broker");

    char status_json[64];
    create_status_json(status_json, 1);

    char status_topic[64];
    snprintf(status_topic, sizeof(status_topic), "devices/%s/status",
             device_id);

    esp_mqtt_client_publish(mqtt_client_handle, status_topic, status_json, 0, 0,
                            1);

    char topic[64];
    snprintf(topic, sizeof(topic), "devices/%s/info", device_id);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "mac", mac_str);
    cJSON_AddStringToObject(root, "ip", ip_str);
    cJSON_AddStringToObject(root, "ssid", MY_ESP_WIFI_SSID);

    char *json = cJSON_PrintUnformatted(root);

    esp_mqtt_client_publish(mqtt_client_handle, topic, json, 0, 0, 1);

    free(json);
    cJSON_Delete(root);

    char cmd_topic[64];
    snprintf(cmd_topic, sizeof(cmd_topic), "devices/%s/command/+", device_id);
    esp_mqtt_client_subscribe(mqtt_client_handle, cmd_topic, 1);

  } break;

  case MQTT_EVENT_DISCONNECTED: {
    ESP_LOGI("MQTT", "disconnected");
  } break;

  case MQTT_EVENT_DATA: {
    ESP_LOGI("MQTT", "data received: %.*s", event->data_len, event->data);
    char onboard_led_command[64];
    snprintf(onboard_led_command, sizeof(onboard_led_command),
             "devices/%s/command/onboard_led", device_id);

    if (event->topic_len == strlen(onboard_led_command) &&
        strncmp(event->topic, onboard_led_command, event->topic_len) == 0) {

      cJSON *json = cJSON_ParseWithLength(event->data, event->data_len);
      if (json) {
        cJSON *status = cJSON_GetObjectItem(json, "status");

        if (cJSON_IsNumber(status)) {
          int status_value = status->valueint;
          char state_topic[64];

          gpio_set_level(GPIO_NUM_2, status_value);

          char state_json[64];
          create_status_json(state_json, status_value);

          snprintf(state_topic, sizeof(state_topic),
                   "devices/%s/state/onboard_led", device_id);

          esp_mqtt_client_publish(mqtt_client_handle, state_topic, state_json,
                                  0, 1, 1);
        }

        cJSON_Delete(json);
      }
    }

  } break;

  default:
    break;
  }
}

void setup_mqtt() {
  char status_json[64];
  create_status_json(status_json, 0);

  gpio_reset_pin(GPIO_NUM_2);
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

  char status_topic[64];
  snprintf(status_topic, sizeof(status_topic), "devices/%s/status", device_id);

  const esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = CONFIG_MQTT_BROKER_URI,
      .session.keepalive = 10,
      .session.last_will = {.topic = status_topic,
                            .msg = status_json,
                            .msg_len = 0,
                            .qos = 1,
                            .retain = true}};

  mqtt_client_handle = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client_handle, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, mqtt_client_handle);
  esp_mqtt_client_start(mqtt_client_handle);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "wifi connecting to %s", MY_ESP_WIFI_SSID);
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "wifi disconnected, reconnecting");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));

    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));

    setup_mqtt();
  }
}

void app_main(void) {
  get_device_id(device_id, sizeof(device_id));
  get_mac_str(mac_str, sizeof(mac_str));
  ESP_LOGI(TAG, "device ID: %s", device_id);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = MY_ESP_WIFI_SSID,
              .password = MY_ESP_WIFI_PASS,
          },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
};
