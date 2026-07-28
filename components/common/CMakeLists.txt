idf_component_register(
    SRCS
        "src/wifi_sta.c"
        "src/mqtt_wrap.c"
        "src/ha_discovery.c"
        "src/app_rtos.c"
        "src/ota.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        mqtt
        driver
        json
        esp_https_ota
        app_update
        esp_app_format
        esp_http_client
        esp-tls
        mbedtls
        esp_timer
)

# TLS only for esp2 (when platformio.ini passes -DMQTT_TLS=1). ESP-IDF needs both
# this target_add_binary_data call AND board_build.embed_txtfiles in the esp2 env
# of platformio.ini (which registers the file with the PlatformIO builder so the
# generated ca.crt.S is produced). TEXT appends a NUL so the PEM works as a C
# string; the link symbol is the basename -> _binary_ca_crt_start (see mqtt_wrap.c).
if(MQTT_TLS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MQTT_USE_TLS=1)
    target_add_binary_data(${COMPONENT_LIB} "certs/ca.crt" TEXT)
endif()
