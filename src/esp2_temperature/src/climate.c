#include "climate.h"
#include "dht22.h"
#include "wifi_sta.h"
#include "mqtt_wrap.h"
#include "ha_discovery.h"
#include "app_rtos.h"

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "cJSON.h"
#include "esp_log.h"

#define BOARD    "temperature"
#define HOSTNAME "esp32-temperature"

static const char *TAG = "climate";
static const char *SENSOR_STATE = "home/temperature/sensor/state";
static const char *HEATER_SET   = "home/temperature/heater/set";
static const char *HEATER_STATE = "home/temperature/heater/state";

static uint8_t s_dht_pin;
static uint8_t s_heater_pin;
static bool s_heater_active_low;
static bool s_heater_on;

static void apply_heater(bool on) {
    s_heater_on = on;
    gpio_set_level(s_heater_pin, (on ^ s_heater_active_low) ? 1 : 0);
    mqtt_publish(HEATER_STATE, on ? "ON" : "OFF", true);
}

static void poll_task(void *ctx) {
    (void)ctx;
    if (!mqtt_is_connected()) return;

    float t = 0, h = 0;
    esp_err_t err = dht22_read(s_dht_pin, &t, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DHT read failed (%d)", err);
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", (int)(t * 10 + 0.5f) / 10.0);
    cJSON_AddNumberToObject(root, "humidity", (int)(h * 10 + 0.5f) / 10.0);
    char *out = cJSON_PrintUnformatted(root);
    if (out) {
        mqtt_publish(SENSOR_STATE, out, true);
        cJSON_free(out);
    }
    cJSON_Delete(root);
}

static void on_message(const char *topic, int topic_len, const char *data, int data_len) {
    if ((int)strlen(HEATER_SET) == topic_len && strncmp(HEATER_SET, topic, topic_len) == 0) {
        apply_heater(data_len == 2 && strncmp(data, "ON", 2) == 0);
    }
}

static void on_connect(void) {
    mqtt_subscribe(HEATER_SET);

    char extra[512];
    snprintf(extra, sizeof(extra),
             "\"state_topic\":\"%s\",\"unit_of_measurement\":\"\\u00b0C\","
             "\"device_class\":\"temperature\","
             "\"value_template\":\"{{ value_json.temperature }}\"", SENSOR_STATE);
    ha_publish_config("sensor", "temperature", extra);

    snprintf(extra, sizeof(extra),
             "\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\","
             "\"device_class\":\"humidity\","
             "\"value_template\":\"{{ value_json.humidity }}\"", SENSOR_STATE);
    ha_publish_config("sensor", "humidity", extra);

    snprintf(extra, sizeof(extra),
             "\"command_topic\":\"%s\",\"state_topic\":\"%s\","
             "\"payload_on\":\"ON\",\"payload_off\":\"OFF\"", HEATER_SET, HEATER_STATE);
    ha_publish_config("switch", "heater", extra);

    mqtt_publish(HEATER_STATE, s_heater_on ? "ON" : "OFF", true);
}

void climate_start(uint8_t dht_pin, uint8_t heater_pin,
                   bool heater_active_low, uint32_t publish_ms) {
    s_dht_pin = dht_pin;
    s_heater_pin = heater_pin;
    s_heater_active_low = heater_active_low;

    gpio_reset_pin(heater_pin);
    gpio_set_direction(heater_pin, GPIO_MODE_OUTPUT);
    apply_heater(false);

    wifi_sta_start(HOSTNAME);
    ha_set_device(BOARD, "ESP2 Temperature");
    mqtt_start(BOARD, on_message, on_connect);

    rtos_every_ms("dht", poll_task, NULL, publish_ms,
                  RTOS_STACK_DEFAULT, RTOS_PRIO_NORMAL, RTOS_CORE_APP);
}
