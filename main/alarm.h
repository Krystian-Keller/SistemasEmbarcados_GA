#ifndef ALARM_H
#define ALARM_H

#include "esp_err.h"

esp_err_t alarm_gpio_init(int led_pin);
void alarm_led_set(int on);

#endif