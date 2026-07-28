#include "ha_discovery.h"
#include "mqtt_wrap.h"

#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"

static char s_board[32]   = "esp32";
static char s_display[48] = "ESP32";

void ha_set_device(const char *board, const char *display_name) {
    strncpy(s_board, board, sizeof(s_board) - 1);
    strncpy(s_display, display_name, sizeof(s_display) - 1);
}

void ha_publish_config(const char *component, const char *object_id,
                       const char *extra_json) {
    char topic[160];
    snprintf(topic, sizeof(topic), "homeassistant/%s/%s/%s/config",
             component, s_board, object_id);

    char json[1024];
    int n = snprintf(json, sizeof(json),
        "{"
        "\"name\":\"%s\","
        "\"unique_id\":\"%s_%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_available\":\"online\","
        "\"payload_not_available\":\"offline\",",
        object_id, s_board, object_id, mqtt_availability_topic());
    /* snprintf returns the would-be length; clamp so sizeof(json) - n never
     * underflows and defeats the bound of the next call. */
    if (n < 0 || n > (int)sizeof(json)) n = sizeof(json);

    if (extra_json && extra_json[0]) {
        n += snprintf(json + n, sizeof(json) - n, "%s,", extra_json);
        if (n < 0 || n > (int)sizeof(json)) n = sizeof(json);
    }

    /* Device block. sw_version comes from the app descriptor (version.txt),
     * so Home Assistant shows the running firmware version for every entity. */
    snprintf(json + n, sizeof(json) - n,
        "\"device\":{"
        "\"identifiers\":[\"esp32_%s\"],"
        "\"name\":\"%s\","
        "\"model\":\"ESP32 DevKit\","
        "\"manufacturer\":\"HomeAssistantOrchestration\","
        "\"sw_version\":\"%s\""
        "}}",
        s_board, s_display, esp_app_get_description()->version);

    mqtt_publish(topic, json, true);
}
