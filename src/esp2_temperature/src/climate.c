/* ESP2 (ESP-WROVER-KIT V4.1): 4 simulated temperature sensors.
 *
 * Instead of a real DHT22, this generates four plausible temperatures with a
 * small random walk, publishes them as one retained JSON payload every
 * publish_ms, and mirrors them on the on-board LCD. Four HA "sensor" entities
 * auto-appear via MQTT Discovery. */
#include "climate.h"
#include "display.h"
#include "wifi_sta.h"
#include "mqtt_wrap.h"
#include "ha_discovery.h"
#include "app_rtos.h"

#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "esp_log.h"

#define BOARD    "temperature"
#define HOSTNAME "esp32-temperature"
#define NUM_SENSORS 4

static const char *TAG = "climate";
static const char *SENSOR_STATE = "home/temperature/sensor/state";

/* Seed each sensor in a different band so the demo shows some spread. */
static float s_temp[NUM_SENSORS] = { 21.0f, 23.5f, 19.5f, 26.5f };

static uint16_t band_color(float t)
{
    if (t < 22.0f) return C_CYAN;
    if (t < 26.0f) return C_GREEN;
    return C_ORANGE;
}

static void update_dummy(void)
{
    for (int i = 0; i < NUM_SENSORS; i++) {
        /* random step in [-0.5, +0.5] deg C */
        float step = ((int32_t)(esp_random() % 21) - 10) * 0.05f;
        s_temp[i] += step;
        if (s_temp[i] < 15.0f) s_temp[i] = 15.0f;
        if (s_temp[i] > 30.0f) s_temp[i] = 30.0f;
    }
}

static void publish_state(void)
{
    char json[128];
    snprintf(json, sizeof(json),
             "{\"t1\":%.1f,\"t2\":%.1f,\"t3\":%.1f,\"t4\":%.1f}",
             s_temp[0], s_temp[1], s_temp[2], s_temp[3]);
    mqtt_publish(SENSOR_STATE, json, true);
}

static void render_header(void)
{
    display_fill_rect(0, 0, LCD_WIDTH, 34, C_NAVY);
    display_text(12, 9, "ESP2 TEMPERATURES", C_WHITE, C_NAVY, 2);
}

static void render_values(void)
{
    for (int i = 0; i < NUM_SENSORS; i++) {
        int y = 50 + i * 46;
        char line[24];
        snprintf(line, sizeof(line), "S%d: %4.1f C", i + 1, s_temp[i]);
        display_fill_rect(0, y - 3, LCD_WIDTH, 30, C_BLACK);
        display_text(10, y, line, band_color(s_temp[i]), C_BLACK, 3);
    }
}

static void render_footer(void)
{
    bool up = mqtt_is_connected();
    display_fill_rect(0, 212, LCD_WIDTH, 26, C_BLACK);
    display_text(10, 214, up ? "MQTT: ONLINE" : "MQTT: OFFLINE",
                 up ? C_GREEN : C_RED, C_BLACK, 2);
}

static void tick(void *ctx)
{
    (void)ctx;
    update_dummy();
    render_values();
    render_footer();
    if (mqtt_is_connected()) {
        publish_state();
    }
}

static void on_message(const char *topic, int topic_len,
                       const char *data, int data_len)
{
    (void)topic; (void)topic_len; (void)data; (void)data_len;
    /* No commands: the sensors are read-only. */
}

static void on_connect(void)
{
    char extra[320];
    for (int i = 0; i < NUM_SENSORS; i++) {
        char object_id[12];
        snprintf(object_id, sizeof(object_id), "temp%d", i + 1);
        snprintf(extra, sizeof(extra),
                 "\"state_topic\":\"%s\","
                 "\"unit_of_measurement\":\"\\u00b0C\","
                 "\"device_class\":\"temperature\","
                 "\"value_template\":\"{{ value_json.t%d }}\"",
                 SENSOR_STATE, i + 1);
        ha_publish_config("sensor", object_id, extra);
    }
    publish_state();
    render_footer();
    ESP_LOGI(TAG, "connected: published %d temperature sensors", NUM_SENSORS);
}

void climate_start(uint32_t publish_ms)
{
    display_init();
    render_header();
    render_values();
    render_footer();

    wifi_sta_start(HOSTNAME);
    ha_set_device(BOARD, "ESP2 Temperature");
    mqtt_start(BOARD, on_message, on_connect);

    rtos_every_ms("temps", tick, NULL, publish_ms,
                  RTOS_STACK_DEFAULT, RTOS_PRIO_NORMAL, RTOS_CORE_APP);
}
