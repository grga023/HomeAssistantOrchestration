#include "ha_discovery.h"
#include "mqtt_wrap.h"

#include <stdio.h>
#include <string.h>

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

    if (extra_json && extra_json[0]) {
        n += snprintf(json + n, sizeof(json) - n, "%s,", extra_json);
    }

    snprintf(json + n, sizeof(json) - n,
        "\"device\":{"
        "\"identifiers\":[\"esp32_%s\"],"
        "\"name\":\"%s\","
        "\"model\":\"ESP32 DevKit\","
        "\"manufacturer\":\"HomeAssistantOrchestration\""
        "}}",
        s_board, s_display);

    mqtt_publish(topic, json, true);
}
