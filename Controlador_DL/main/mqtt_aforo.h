#ifndef MQTT_AFORO_H
#define MQTT_AFORO_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "cJSON.h"

// Variables externas necesarias del código
extern esp_mqtt_client_handle_t client;
extern esp_timer_handle_t timer_handle;

#ifdef __cplusplus
extern "C"
{
#endif
    // Cabeceras de funciones
    void publish_value(int value, char key[10]);
    void log_error_if_nonzero(const char *message, int error_code);
    void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void mqtt_app_start(void);

#ifdef __cplusplus
}
#endif

#endif // MQTT_AFORO_H
