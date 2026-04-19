idf_component_register(
    SRCS
    "main.c"
    "alarm.c"
    "adxl345.c"
    "wifi.c"
   "mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c esp_wifi esp_event esp_netif nvs_flash mqtt mbedtls esp-tls
)