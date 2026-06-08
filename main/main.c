#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "alarm.h"
#include "adxl345.h"
#include "wifi.h"
#include "mqtt.h"

#define LED_PIN 2

#define WIFI_SSID      "Eduardo"
#define WIFI_PASSWORD  "escoteiro12"

#define MQTT_BROKER_URI "mqtts://broker.hivemq.com:1883"

#define ALARM_TIMEOUT_MS      5000 
#define SAMPLE_PERIOD_MS      500
#define MOVEMENT_THRESHOLD_G  0.03f

static const char *TAG = "MAIN";

typedef enum {
    STATE_UNKNOWN = 0,
    STATE_MOVIMENTO,
    STATE_PARADO,
    STATE_ALARME
} system_state_t;

typedef struct {
    system_state_t current_status;
    SemaphoreHandle_t status_mutex;
} app_context_t;

static const char *state_to_string(system_state_t state)
{
    switch (state) {
        case STATE_MOVIMENTO: return "MOVIMENTO";
        case STATE_PARADO:    return "PARADO";
        case STATE_ALARME:    return "ALARME";
        default:              return "UNKNOWN";
    }
}

static void Produz(void *pvParameters)
{
    app_context_t *ctx = (app_context_t *)pvParameters;

    esp_err_t err;
    float x_g = 0.0f, y_g = 0.0f, z_g = 0.0f;
    float magnitude = 0.0f;
    float last_magnitude = 0.0f;
    float delta = 0.0f;
    bool first_sample = true;

    TickType_t last_movement_tick = xTaskGetTickCount();

    while (1) {
        err = adxl345_read_g(&x_g, &y_g, &z_g);

        if (err == ESP_OK) {
            magnitude = sqrtf((x_g * x_g) + (y_g * y_g) + (z_g * z_g));

            TickType_t now_tick = xTaskGetTickCount();
            uint32_t idle_time_ms = (uint32_t)pdTICKS_TO_MS(now_tick - last_movement_tick);
            system_state_t new_state = STATE_UNKNOWN;

            if (first_sample) {
                last_magnitude = magnitude;
                first_sample = false;
                last_movement_tick = now_tick;
                idle_time_ms = 0;
                delta = 0.0f;
                new_state = STATE_PARADO;

                ESP_LOGI(TAG,
                         "[Produz] Primeira leitura | X: %.3f g | Y: %.3f g | Z: %.3f g | MAG: %.3f g",
                         x_g, y_g, z_g, magnitude);
            } else {
                delta = fabsf(magnitude - last_magnitude);
                idle_time_ms = (uint32_t)pdTICKS_TO_MS(now_tick - last_movement_tick);

                if (delta >= MOVEMENT_THRESHOLD_G) {
                    last_movement_tick = now_tick;
                    idle_time_ms = 0;
                    new_state = STATE_MOVIMENTO;

                    ESP_LOGI(TAG,
                             "[Produz] MOVIMENTO | X: %.3f g | Y: %.3f g | Z: %.3f g | MAG: %.3f g | DELTA: %.3f g | idle: %lu ms",
                             x_g, y_g, z_g, magnitude, delta, (unsigned long)idle_time_ms);
                } else {
                    new_state = STATE_PARADO;

                    ESP_LOGI(TAG,
                             "[Produz] PARADO    | X: %.3f g | Y: %.3f g | Z: %.3f g | MAG: %.3f g | DELTA: %.3f g | idle: %lu ms",
                             x_g, y_g, z_g, magnitude, delta, (unsigned long)idle_time_ms);
                }

                if (idle_time_ms >= ALARM_TIMEOUT_MS) {
                    new_state = STATE_ALARME;
                    alarm_led_set(1);
                    ESP_LOGW(TAG, "[Produz] ALARME LIGADO! Sem movimento por %lu ms",
                             (unsigned long)idle_time_ms);
                } else {
                    alarm_led_set(0);
                }

                last_magnitude = magnitude;
            }

            if (xSemaphoreTake(ctx->status_mutex, portMAX_DELAY) == pdTRUE) {
                ctx->current_status = new_state;
                xSemaphoreGive(ctx->status_mutex);
            }
        } else {
            ESP_LOGE(TAG, "[Produz] Erro ao ler ADXL345");
            alarm_led_set(1);

            if (xSemaphoreTake(ctx->status_mutex, portMAX_DELAY) == pdTRUE) {
                ctx->current_status = STATE_UNKNOWN;
                xSemaphoreGive(ctx->status_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

static void Consome(void *pvParameters)
{
    app_context_t *ctx = (app_context_t *)pvParameters;

    esp_err_t err;
    bool mqtt_connected_logged = false;
    bool mqtt_initial_sent = false;
    system_state_t last_published_state = STATE_UNKNOWN;

    while (1) {
        if (mqtt_is_connected() && !mqtt_connected_logged) {
            mqtt_connected_logged = true;
            ESP_LOGI(TAG, "[Consome] MQTT conectado de verdade ao broker");
        }

        if (mqtt_is_connected() && !mqtt_initial_sent) {
            err = mqtt_publish_status("ESP32_CONECTADO");
            if (err == ESP_OK) {
                mqtt_initial_sent = true;
                ESP_LOGI(TAG, "[Consome] Mensagem inicial MQTT publicada");
            } else {
                ESP_LOGW(TAG, "[Consome] Falha ao publicar mensagem inicial MQTT");
            }
        }

        system_state_t current_status = STATE_UNKNOWN;

        if (xSemaphoreTake(ctx->status_mutex, portMAX_DELAY) == pdTRUE) {
            current_status = ctx->current_status;
            xSemaphoreGive(ctx->status_mutex);
        }

        if (mqtt_is_connected() &&
            current_status != STATE_UNKNOWN &&
            current_status != last_published_state) {

            const char *payload = state_to_string(current_status);
            printf("aiaai");
            err = mqtt_publish_status(payload);

            if (err == ESP_OK) {
                ESP_LOGI(TAG, "[Consome] Estado publicado no MQTT: %s", payload);
                last_published_state = current_status;
            } else {
                ESP_LOGW(TAG, "[Consome] Falha ao publicar estado no MQTT: %s", payload);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void)
{
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(alarm_gpio_init(LED_PIN));

    err = wifi_init_sta(WIFI_SSID, WIFI_PASSWORD);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao conectar no Wi-Fi");
        while (1) {
            alarm_led_set(1);
            vTaskDelay(pdMS_TO_TICKS(150));
            alarm_led_set(0);
            vTaskDelay(pdMS_TO_TICKS(150));
        }
    }

    ESP_LOGI(TAG, "Wi-Fi conectado com sucesso");

    err = mqtt_app_start(MQTT_BROKER_URI);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar MQTT");
        while (1) {
            alarm_led_set(1);
            vTaskDelay(pdMS_TO_TICKS(300));
            alarm_led_set(0);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }

    ESP_LOGI(TAG, "MQTT iniciado com sucesso");

    err = adxl345_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar ADXL345");
        while (1) {
            alarm_led_set(1);
            vTaskDelay(pdMS_TO_TICKS(200));
            alarm_led_set(0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    static app_context_t app_ctx = {
        .current_status = STATE_UNKNOWN,
        .status_mutex = NULL
    };

    app_ctx.status_mutex = xSemaphoreCreateMutex();
    if (app_ctx.status_mutex == NULL) {
        ESP_LOGE(TAG, "Falha ao criar mutex de status");
        while (1) {
            alarm_led_set(1);
            vTaskDelay(pdMS_TO_TICKS(100));
            alarm_led_set(0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    BaseType_t ret_prod = xTaskCreate(
        Produz,
        "Produz",
        4096,
        &app_ctx,
        5,
        NULL
    );

    BaseType_t ret_cons = xTaskCreate(
        Consome,
        "Consome",
        4096,
        &app_ctx,
        4,
        NULL
    );

    if (ret_prod != pdPASS || ret_cons != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar as tasks Produz/Consome");
        while (1) {
            alarm_led_set(1);
            vTaskDelay(pdMS_TO_TICKS(100));
            alarm_led_set(0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ESP_LOGI(TAG, "Tasks Produz e Consome iniciadas com sucesso");
}
