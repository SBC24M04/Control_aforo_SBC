set(src_files       app_main.cpp
                    wifi_aforo.c
                    mqtt_aforo.c)

set(include_dirs    ./ . ./../)

set(requires        pedestrian_detect
                    esp_jpeg
                    nvs_flash
                    esp_wifi
                    mqtt
                    json
                    esp_http_server)

idf_component_register(SRCS ${src_files} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
