#include "alarm.h"
#include "driver/gpio.h"

static int s_led_pin;

esp_err_t alarm_gpio_init(int led_pin)
{
    s_led_pin = led_pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_led_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    return gpio_config(&io_conf);
}

void alarm_led_set(int on)
{
    gpio_set_level(s_led_pin, on);
}