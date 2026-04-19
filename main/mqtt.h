#ifndef MQTT_H
#define MQTT_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t mqtt_app_start(const char *broker_uri);
esp_err_t mqtt_publish_status(const char *status);
bool mqtt_is_connected(void);

#endif