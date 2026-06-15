idf_component_register(SRCS "main.c" "wifi.c" "mqtt.c" "json.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_driver_gpio
                    PRIV_REQUIRES esp_netif
                    PRIV_REQUIRES nvs_flash 
                    PRIV_REQUIRES esp_http_server
                    PRIV_REQUIRES esp_wifi 
                    PRIV_REQUIRES esp_adc
)
