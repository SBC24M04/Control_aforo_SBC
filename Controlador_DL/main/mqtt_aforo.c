#include "mqtt_aforo.h"

static const char *TAG = "MQTT aforo";
esp_mqtt_client_handle_t client = NULL;

void publish_value(int value, char key[10])
{
    if (client)
    {
        // Crea un objeto JSON para el valor del ADC
        cJSON *root = cJSON_CreateObject();
        if (root == NULL)
        {
            ESP_LOGE(TAG, "Failed to create JSON object");
            return;
        }

        cJSON_AddNumberToObject(root, key, value);
        char *post_data = cJSON_PrintUnformatted(root);
        if (post_data == NULL)
        {
            ESP_LOGE(TAG, "Failed to print JSON data");
            cJSON_Delete(root);
            return;
        }

        // Publica los datos en ThingsBoard
        int msg_id = esp_mqtt_client_publish(client, "v1/devices/me/telemetry", post_data, 0, 1, 0);
        if (msg_id == -1)
        {
            ESP_LOGE(TAG, "Failed to publish message");
        }
        else
        {
            ESP_LOGI(TAG, "Published %s to MQTT: %i", key, value);
        }

        // Libera la memoria de los objetos JSON
        cJSON_Delete(root);
        free(post_data);
    }
    else
    {
        ESP_LOGE(TAG, "MQTT client is not initialized");
    }
}

void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    if (event == NULL)
    {
        ESP_LOGE(TAG, "event_data is NULL");
        return;
    }
    esp_mqtt_client_handle_t client = event->client;
    if (client == NULL)
    {
        ESP_LOGE(TAG, "client is NULL");
        return;
    }
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // xTaskCreate(publish_value, "publishtask", 4096, NULL, 5, NULL);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle != NULL && event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = "mqtt://demo.thingsboard.io";
    mqtt_cfg.broker.address.port = 1883;
    mqtt_cfg.credentials.username = "";

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }
    esp_err_t err = esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(err));
        return;
    }
    esp_mqtt_client_start(client);
}