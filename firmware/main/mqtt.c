#include "mqtt.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "json.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static esp_mqtt_client_handle_t mqtt_client_handle;
static const char *s_id, *s_mac, *s_ip;

void mqtt_publish(const char *topic, const char *data, int len, int qos,
                  int retain) {
  esp_mqtt_client_publish(mqtt_client_handle, topic, data, len, qos, retain);
}

static void mqtt_event_handler(void *event_handler_arg,
                               esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {

  esp_mqtt_event_handle_t event = event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED: {
    ESP_LOGI("MQTT", "Connected to broker");

    char status_json[64];
    create_status_json(status_json, 1);

    char status_topic[64];
    snprintf(status_topic, sizeof(status_topic), "devices/%s/status", s_id);

    mqtt_publish(status_topic, status_json, 0, 0, 1);

    char topic[64];
    snprintf(topic, sizeof(topic), "devices/%s/info", s_id);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "device_id", s_id);
    cJSON_AddStringToObject(root, "mac", s_mac);
    cJSON_AddStringToObject(root, "ip", s_ip);
    cJSON_AddStringToObject(root, "ssid", CONFIG_ESP_WIFI_SSID);

    char *json = cJSON_PrintUnformatted(root);

    mqtt_publish(topic, json, 0, 0, 1);

    free(json);
    cJSON_Delete(root);

    char cmd_topic[64];
    snprintf(cmd_topic, sizeof(cmd_topic), "devices/%s/command/+", s_id);
    esp_mqtt_client_subscribe(mqtt_client_handle, cmd_topic, 1);

  } break;

  case MQTT_EVENT_DISCONNECTED: {
    ESP_LOGI("MQTT", "disconnected");
  } break;

  case MQTT_EVENT_DATA: {
    ESP_LOGI("MQTT", "data received: %.*s", event->data_len, event->data);
    char onboard_led_command[64];
    snprintf(onboard_led_command, sizeof(onboard_led_command),
             "devices/%s/command/onboard_led", s_id);

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
                   "devices/%s/state/onboard_led", s_id);

          mqtt_publish(state_topic, state_json, 0, 1, 1);
        }

        cJSON_Delete(json);
      }
    }

  } break;

  default:
    break;
  }
}

void mqtt_init(const char *device_id, const char *mac, const char *ip) {
  s_id = device_id;
  s_mac = mac;
  s_ip = ip;

  char status_json[64];
  create_status_json(status_json, 0);

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
