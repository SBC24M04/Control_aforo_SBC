#include "esp_log.h"
#include "esp_timer.h"
#include "jpeg_decoder.h"
#include "pedestrian_detect.hpp"
#include "esp_camera.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <esp_system.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "driver/adc_types_legacy.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <time.h>
#include "esp_timer.h"
#include "esp_mac.h"

#include "wifi_aforo.h"
#include "mqtt_aforo.h"

#pragma GCC optimize("Ofast")

#define MAX_APS 20

extern const uint8_t pedestrian_jpg_start[] asm("_binary_pedestrian_jpg_start");
extern const uint8_t pedestrian_jpg_end[] asm("_binary_pedestrian_jpg_end");

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

const char *TAG = "pedestrian_detect";

#define MEDIO_IMAGEN 320

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11

#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

int value = 0;

int IzquierdaGlobal = 0;
int DerechaGlobal = 0;

int flagDerecha = 0;
int flagIzquierda = 0;

int CantidadPersonas = 0;

typedef struct
{
    float score;
    int box[4];
} DetectionResult;

static esp_err_t init_camera(void)
{
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 2000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,

        .jpeg_quality = 10,
        .fb_count = 4,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST};
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al inicializar la cámara: 0x%x", err);
        if (err == ESP_ERR_CAMERA_NOT_DETECTED)
        {
            ESP_LOGE(TAG, "Cámara no detectada. Verifique la conexión y los pines.");
        }
        else if (err == ESP_ERR_CAMERA_FAILED_TO_SET_FRAME_SIZE)
        {
            ESP_LOGE(TAG, "Error al configurar el tamaño del frame.");
        }
        else
        {
            ESP_LOGE(TAG, "Error desconocido al inicializar la cámara.");
        }
        return err;
    }
    sensor_t *camera = esp_camera_sensor_get();
    camera->set_exposure_ctrl(camera, 0);
    camera->set_aec_value(camera, 20);
    ESP_LOGI(TAG, "Cámara inicializada correctamente");
    return ESP_OK;
}

void clearFlags()
{
    flagDerecha = 0;
    flagIzquierda = 0;
}

void countAforo(auto detect_results)
{
    std::list<int> x;
    int flagIzquierda2 = 0, flagDerecha2 = 0;
    for (const auto &res : detect_results)
    {
        x.push_back((res.box[2] + res.box[0]) / 2);
    }

    for (int i : x)
    {
        bool esIzquierda = (i <= MEDIO_IMAGEN);
        bool esDerecha = !esIzquierda;

        if (x.size() >= 2)
        {
            if (esDerecha && flagIzquierda)
            {
                CantidadPersonas--;
            }
            else if (esIzquierda && flagDerecha)
            {
                CantidadPersonas++;
            }
            else if (esIzquierda)
            {
                if (flagIzquierda2)
                    flagIzquierda = 1;
                flagIzquierda2 = 1;
            }
            else if (esDerecha)
            {
                if (flagDerecha2)
                    flagDerecha = 1;
                flagDerecha2 = 1;
            }
        }
        else if (x.size() == 1)
        {
            if (esDerecha && flagIzquierda)
            {
                CantidadPersonas--;
                clearFlags();
            }
            else if (esIzquierda && flagDerecha)
            {
                CantidadPersonas++;
                clearFlags();
            }
            else
            {
                esIzquierda ? flagIzquierda = 1 : flagDerecha = 1;
            }
        }
    }

    if (x.empty())
    {
        clearFlags();
    }
    CantidadPersonas = std::max(CantidadPersonas, 0);
    publish_value(CantidadPersonas, "aforo");
}

esp_err_t jpg_stream_httpd_handler()
{
    ESP_LOGI(TAG, "HTTP Stream Handler Started");

    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[64];
    static int64_t last_frame = 0;

    if (!last_frame)
    {
        last_frame = esp_timer_get_time();
    }

    if (res != ESP_OK)
    {
        return res;
    }

    PedestrianDetect *detect = new PedestrianDetect();

    while (true)
    {
        fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        dl::image::jpeg_img_t jpeg_img = {
            .data = fb->buf,
            .width = int(fb->width),
            .height = int(fb->height),
            .data_size = fb->len,
        };

        dl::image::img_t img;
        img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
        sw_decode_jpeg(jpeg_img, img);

        auto &detect_results = detect->run(img);

        if (0 < detect_results.size() || detect_results.size() < 5)
        {
            countAforo(detect_results);
        }

        int border_thickness = 2;
        auto *rgb_data = static_cast<uint8_t *>(img.data);
        value = 0;

        for (const auto &res : detect_results)
        {
            value++;
        }

        if (value <= 0)
        {
            clearFlags();
        }

        publish_value(value, "value");
        esp_camera_fb_return(fb);
        heap_caps_free(img.data);

        if (res != ESP_OK)
        {
            break;
        }

        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        ESP_LOGI(TAG, "MJPG: %lums (%.1ffps)",
                 (uint32_t)frame_time,
                 1000.0 / (uint32_t)frame_time);
    }

    delete detect;
    last_frame = 0;
    return res;
}

static void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if (*src == '%')
        {
            if (isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2]))
            {
                a = (src[1] >= 'A') ? (toupper(src[1]) - 'A' + 10) : (src[1] - '0');
                b = (src[2] >= 'A') ? (toupper(src[2]) - 'A' + 10) : (src[2] - '0');
                *dst++ = (a << 4) | b;
                src += 3;
            }
            else
            {
                *dst++ = *src++;
            }
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static esp_err_t set_wifi_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        buf[ret] = '\0';
        ESP_LOGI(TAG, "Received data: %s", buf);
    }

    char ssid[32], password[64], decoded_buf[100];
    url_decode(decoded_buf, buf);

    sscanf(decoded_buf, "ssid=%[^&]&password=%s", ssid, password);

    save_wifi_credentials(ssid, password);

    const char resp[] = "WiFi credentials received. Restarting ESP32...";
    httpd_resp_send(req, resp, strlen(resp));

    esp_restart();
    return ESP_OK;
}

static esp_err_t password_pass(httpd_req_t *req)
{
    scan_wifi_aps();

    char form[2024];

    snprintf(form, sizeof(form),
             "<html><body><h1>Configure your WiFi</h1>"
             "<form action=\"/set_wifi\" method=\"post\">"
             "Select SSID: <select name=\"ssid\">");

    for (int i = 0; i < ap_count; i++)
    {
        snprintf(form + strlen(form), sizeof(form) - strlen(form),
                 "<option value=\"%s\">%s</option>", wifi_ap_list[i].ssid, wifi_ap_list[i].ssid);
    }

    snprintf(form + strlen(form), sizeof(form) - strlen(form),
             "</select><br>"
             "Password: <input type=\"password\" name=\"password\"><br>"
             "<input type=\"submit\" value=\"Connect\">"
             "</form></body></html>");

    httpd_resp_send(req, form, strlen(form));
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = password_pass,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t set_wifi_uri = {
            .uri = "/set_wifi",
            .method = HTTP_POST,
            .handler = set_wifi_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &set_wifi_uri);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}

extern "C" void app_main(void)
{
    wifi_init();
    wifi_connect_from_nvs();

    while (booting && !station_mode)
    {
        ESP_LOGI(TAG, "Intentando conectar...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    if (!station_mode)
    {
        start_webserver();
        while (true)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    else
    {

        init_camera();
        mqtt_app_start();
    }
    jpg_stream_httpd_handler();
}
