#ifndef WIFI_AFORO_H
#define WIFI_AFORO_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

// Definiciones
#define EXAMPLE_ESP_WIFI_SSID "espd1bc"
#define EXAMPLE_ESP_WIFI_PASS "12345678"
#define EXAMPLE_MAX_STA_CONN 4
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX_APS 20
#define MQTT_PUBLISH_INTERVAL_MS 5000
#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64
#define MQTT_PUBLISH_INTERVAL_S 15
#define ADC_READ_INTERVAL_S 10

extern bool station_mode;
extern int out;
extern bool booting;
extern bool got_ip;
extern bool value_plublished;

// Estructuras
typedef struct
{
    char ssid[32];
} wifi_ap_t;

extern wifi_ap_t wifi_ap_list[MAX_APS];
extern int ap_count;
extern esp_netif_t *wifi_netif;

#ifdef __cplusplus
extern "C"
{
#endif
    // Declaración de funciones
    void wifi_searchmode(void);
    void scan_wifi_aps(void);
    esp_err_t save_wifi_credentials(const char *ssid, const char *password);
    void wifi_init_softap(void);
    void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    void timeout_callback(void *arg);
    void wifi_connect(const char *ssid, const char *password);
    void wifi_connect_from_nvs(void);
    void wifi_init(void);
#ifdef __cplusplus
}
#endif

#endif // WIFI_AFORO_H
