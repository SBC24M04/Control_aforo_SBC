#include "wifi_aforo.h"

static const char *TAG = "WIFI aforo";
esp_timer_handle_t timer_handle;

bool station_mode = false;
bool booting = false;
bool got_ip = false;
bool value_plublished = false;
int out = 0;

wifi_ap_t wifi_ap_list[MAX_APS];
int ap_count = 0;
esp_netif_t *wifi_netif = NULL;

void wifi_searchmode(void)
{

    if (wifi_netif != NULL)
    {
        esp_netif_destroy(wifi_netif);
        wifi_netif = NULL;
    }

    wifi_netif = esp_netif_create_default_wifi_sta();
    if (wifi_netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create default wifi sta netif");
        return;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    // ESP_LOGI(TAG, "Modo estación inicializado.");
}

void scan_wifi_aps(void)
{
    if (booting)
    {
        wifi_searchmode();
    }

    vTaskDelay(4000 / portTICK_PERIOD_MS);

    ap_count = 0;
    uint16_t number = MAX_APS;

    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(err));
        return;
    }

    wifi_ap_record_t ap_info[MAX_APS];

    err = esp_wifi_scan_get_ap_records(&number, ap_info);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(err));
        return;
    }

    ap_count = number > MAX_APS ? MAX_APS : number;

    for (int i = 0; i < ap_count; i++)
    {
        strncpy(wifi_ap_list[i].ssid, (const char *)ap_info[i].ssid, sizeof(wifi_ap_list[i].ssid) - 1);
        wifi_ap_list[i].ssid[sizeof(wifi_ap_list[i].ssid) - 1] = '\0';
    }

    if (booting)
    {
        esp_wifi_stop();
    }
}

esp_err_t save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_creds", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle!");
        return err;
    }

    char key[32];
    char value[100];

    if (strlen(ssid) > 15)
    {
        snprintf(key, sizeof(key), "wifi_%.*s", 9, ssid + (strlen(ssid) - 9));
    }
    else
    {
        snprintf(key, sizeof(key), "wifi_%s", ssid);
    }

    snprintf(value, sizeof(value), "%s,%s", ssid, password);

    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write credentials to NVS!");
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit changes in NVS!");
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

void wifi_init_softap(void)
{

    if (wifi_netif != NULL)
    {
        esp_netif_destroy(wifi_netif);
        wifi_netif = NULL;
    }

    wifi_netif = esp_netif_create_default_wifi_ap();
    if (wifi_netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create default wifi sta netif");
        return;
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = 1,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    booting = false;
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Wi-Fi starting");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Wi-Fi Disconnected, retrying connection");
            got_ip = false;
            if (!booting)
            {
                esp_wifi_connect();
            }
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Connected to WiFi, waiting for IP...");
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            got_ip = true;
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Mask: " IPSTR ", Gateway: " IPSTR,
                     IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw));
            station_mode = true;
            esp_timer_stop(timer_handle);
            booting = false;
            break;
        default:
            break;
        }
    }
}

void timeout_callback(void *arg)
{
    if (!station_mode)
    {
        ESP_LOGW(TAG, "Connection timed out, switching to AP mode.");
        esp_wifi_stop();
        wifi_init_softap();
    }
}

void wifi_connect(const char *ssid, const char *password)
{
    if (wifi_netif != NULL)
    {
        esp_netif_destroy(wifi_netif);
        wifi_netif = NULL;
    }

    wifi_netif = esp_netif_create_default_wifi_sta();
    if (wifi_netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create default wifi sta netif");
        return;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    strncpy((char *)wifi_config.sta.ssid, ssid, MAX_SSID_LEN);
    strncpy((char *)wifi_config.sta.password, password, 64);
    wifi_config.sta.ssid[MAX_SSID_LEN - 1] = '\0';
    wifi_config.sta.password[64 - 1] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);

    const esp_timer_create_args_t timer_args = {
        .callback = &timeout_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_timeout"};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_once(timer_handle, 30000000));
}

void wifi_connect_from_nvs()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_creds", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for reading!");
        wifi_init_softap();
        return;
    }

    booting = true;

    scan_wifi_aps();

    for (int i = 0; i < ap_count; i++)
    {
        ESP_LOGI(TAG, "SSID[%d]: %s", i, wifi_ap_list[i].ssid);
    }

    size_t password_len;
    char key[64], ssid[32], password[64];
    char value[100] = {0};

    for (int i = 0; i < ap_count; i++)
    {

        if (strlen(wifi_ap_list[i].ssid) <= 15)
        {
            snprintf(key, sizeof(key), "wifi_%s", wifi_ap_list[i].ssid);
        }
        else
        {
            snprintf(key, sizeof(key), "wifi_%.*s", 9, wifi_ap_list[i].ssid + (strlen(wifi_ap_list[i].ssid) - 9));
        }

        ESP_LOGI(TAG, "Searching for credentials in NVS for SSID: %s", wifi_ap_list[i].ssid);

        password_len = sizeof(value);
        err = nvs_get_str(nvs_handle, key, value, &password_len);
        if (err == ESP_OK)
        {

            char *comma_pos = strchr(value, ',');
            if (comma_pos != NULL)
            {
                *comma_pos = '\0';

                if (strlen(value) < MAX_SSID_LEN)
                {
                    strncpy(ssid, value, MAX_SSID_LEN);
                    ssid[MAX_SSID_LEN - 1] = '\0';
                }
                else
                {
                    ESP_LOGW(TAG, "SSID is too long, skipping: %s", value);
                    continue;
                }

                strncpy(password, comma_pos + 1, MAX_PASSWORD_LEN);
                password[MAX_PASSWORD_LEN - 1] = '\0';

                ESP_LOGI(TAG, "Found credentials for SSID: %s", ssid);
                ESP_LOGI(TAG, "password is : %s", password);
                wifi_connect(ssid, password);
                nvs_close(nvs_handle);
                return;
            }
        }
        else if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGW(TAG, "No credentials found in NVS for SSID: %s", wifi_ap_list[i].ssid);
        }
        else
        {
            ESP_LOGE(TAG, "Error reading credentials from NVS for SSID: %s, error: %s", wifi_ap_list[i].ssid, esp_err_to_name(err));
        }
    }

    ESP_LOGI(TAG, "No known networks found, switching to AP mode.");
    wifi_init_softap();
    nvs_close(nvs_handle);
}

void wifi_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}