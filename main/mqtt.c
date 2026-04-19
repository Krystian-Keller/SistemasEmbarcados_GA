#include "mqtt.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch ((esp_mqtt_event_id_t) event_id) {
        case MQTT_EVENT_CONNECTED:
            s_mqtt_connected = true;
            ESP_LOGI(TAG, "Conectado ao broker MQTT");
            break;

        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_connected = false;
            ESP_LOGW(TAG, "Desconectado do broker MQTT");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erro no cliente MQTT");
            break;

        default:
            break;
    }
}

esp_err_t mqtt_app_start(const char *broker_uri)
{
    if (broker_uri == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Falha ao criar cliente MQTT");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            s_mqtt_client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));

    ESP_LOGI(TAG, "Cliente MQTT iniciado para broker: %s", broker_uri);
    return ESP_OK;
}

esp_err_t mqtt_publish_status(const char *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_mqtt_connected) {
        ESP_LOGW(TAG, "MQTT ainda não conectado, publish de status ignorado");
        return ESP_ERR_INVALID_STATE;
    }
    printf("oi");
    int msg_id = esp_mqtt_client_publish(
        s_mqtt_client,
        "Scholz/esp32/status",
        status,
        0,
        1,   // QoS
        1    // retain
    );

    if (msg_id == -1) {
        ESP_LOGE(TAG, "Falha ao publicar status MQTT");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Publicado no tópico Scholz/esp32/status: %s (msg_id=%d)", status, msg_id);
    return ESP_OK;
}

bool mqtt_is_connected(void)
{
    return s_mqtt_connected;
}