#include "lights.h"
#include "wifi_sta.h"
#include "mqtt_wrap.h"
#include "ha_discovery.h"

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"

#define MAX_RELAYS 8
#define BOARD      "lights"
#define HOSTNAME   "esp32-lights"

static const uint8_t *s_pins;
static size_t s_count;
static bool s_active_low;
static bool s_state[MAX_RELAYS];

static void cmd_topic(size_t i, char *out, size_t n)   { snprintf(out, n, "home/lights/light%u/set",   (unsigned)(i + 1)); }
static void state_topic(size_t i, char *out, size_t n) { snprintf(out, n, "home/lights/light%u/state", (unsigned)(i + 1)); }

static void apply_relay(size_t i, bool on) {
    s_state[i] = on;
    gpio_set_level(s_pins[i], (on ^ s_active_low) ? 1 : 0);
    char t[48];
    state_topic(i, t, sizeof(t));
    mqtt_publish(t, on ? "ON" : "OFF", true);
}

static void on_message(const char *topic, int topic_len, const char *data, int data_len) {
    char t[48];
    for (size_t i = 0; i < s_count; i++) {
        cmd_topic(i, t, sizeof(t));
        if ((int)strlen(t) == topic_len && strncmp(t, topic, topic_len) == 0) {
            bool on = (data_len == 2 && strncmp(data, "ON", 2) == 0);
            apply_relay(i, on);
            return;
        }
    }
}

static void on_connect(void) {
    char cmd[48], state[48], extra[256];
    char object[16];
    for (size_t i = 0; i < s_count; i++) {
        cmd_topic(i, cmd, sizeof(cmd));
        state_topic(i, state, sizeof(state));
        mqtt_subscribe(cmd);

        snprintf(extra, sizeof(extra),
                 "\"command_topic\":\"%s\",\"state_topic\":\"%s\","
                 "\"payload_on\":\"ON\",\"payload_off\":\"OFF\"",
                 cmd, state);
        snprintf(object, sizeof(object), "light%u", (unsigned)(i + 1));
        ha_publish_config("switch", object, extra);

        mqtt_publish(state, s_state[i] ? "ON" : "OFF", true);
    }
}

void lights_start(const uint8_t *pins, size_t count, bool active_low) {
    s_pins = pins;
    s_count = count > MAX_RELAYS ? MAX_RELAYS : count;
    s_active_low = active_low;

    for (size_t i = 0; i < s_count; i++) {
        gpio_reset_pin(s_pins[i]);
        gpio_set_direction(s_pins[i], GPIO_MODE_OUTPUT);
        apply_relay(i, false);  /* start OFF */
    }

    wifi_sta_start(HOSTNAME);
    ha_set_device(BOARD, "ESP1 Lights");
    mqtt_start(BOARD, on_message, on_connect);
}
