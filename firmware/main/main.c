#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/idf_additions.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"

#include "json.h"
#include "mqtt.h"
#include "wifi.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "main";

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

static void led_init() {
  gpio_reset_pin(GPIO_NUM_2);
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
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
    mqtt_publish(topic, json, 0, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

static void on_ip_ready(const char *ip) {
  strncpy(ip_str, ip, sizeof(ip_str) - 1);
  ip_str[sizeof(ip_str) - 1] = '\0';

  static bool started = false;
  if (started)
    return;
  started = true;

  mqtt_init(device_id, mac_str, ip);

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
