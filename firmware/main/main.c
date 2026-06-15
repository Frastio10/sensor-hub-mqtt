#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/idf_additions.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include "soc/gpio_num.h"

#include "wifi.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "main";
static esp_mqtt_client_handle_t mqtt_client_handle;

static char device_id[32];
static char mac_str[32];
static char ip_str[16];

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

void create_value_json(char *value_str, int value) {
  cJSON *value_root = cJSON_CreateObject();
  cJSON_AddNumberToObject(value_root, "value", value);
  char *json = cJSON_PrintUnformatted(value_root);
  strcpy(value_str, json);

  free(json);
  cJSON_Delete(value_root);
}

static void led_init() {

  gpio_reset_pin(GPIO_NUM_2);
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
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
    cJSON_AddStringToObject(root, "ssid", CONFIG_ESP_WIFI_SSID);

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

void adc_task(void *pvParameters) {
  adc_oneshot_unit_handle_t adc_handle;
  const adc_oneshot_unit_init_cfg_t adc_init_cfg = {
      .unit_id = ADC_UNIT_1,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_cfg, &adc_handle));

  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_12,
      .atten = ADC_ATTEN_DB_12,
  };
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &config));

  int pot_value = 0;

  char topic[64];
  snprintf(topic, sizeof(topic), "devices/%s/telemetry/pot", device_id);

  char json[64];

  while (1) {
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &pot_value));
    ESP_LOGI(TAG, "Potentiometer Value: %d", pot_value);

    create_value_json(json, pot_value);
    esp_mqtt_client_publish(mqtt_client_handle, topic, json, 0, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void setup_mqtt() {
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

static void on_ip_ready(const char *ip) {
  strncpy(ip_str, ip, sizeof(ip_str) - 1);
  ip_str[sizeof(ip_str) - 1] = '\0';

  static bool started = false;
  if (started)
    return;
  started = true;

  setup_mqtt();

  TaskHandle_t xHandle = NULL;
  xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, &xHandle);
}

void app_main(void) {
  get_device_id(device_id, sizeof(device_id));
  get_mac_str(mac_str, sizeof(mac_str));

  led_init();
  ESP_LOGI(TAG, "device ID: %s", device_id);

  wifi_init(on_ip_ready);
};
