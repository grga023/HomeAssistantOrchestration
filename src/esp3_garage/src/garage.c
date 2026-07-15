#include "garage.h"
#include "wifi_sta.h"
#include "mqtt_wrap.h"
#include "ha_discovery.h"
#include "app_rtos.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define BOARD    "garage"
#define HOSTNAME "esp32-garage"

static const char *DOOR_SET   = "home/garage/door/set";
static const char *DOOR_STATE = "home/garage/door/state";

typedef enum { CLOSED, OPEN, OPENING, CLOSING } door_state_t;

static uint8_t s_relay_pin;
static uint8_t s_reed_pin;
static bool s_relay_active_low;
static uint32_t s_pulse_ms;
static uint32_t s_travel_ms;
static door_state_t s_state = CLOSED;
static int64_t s_move_started_ms;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static bool reed_closed(void) { return gpio_get_level(s_reed_pin) == 0; }  /* active-LOW */

static const char *state_str(door_state_t s) {
    switch (s) {
        case OPEN:    return "open";
        case OPENING: return "opening";
        case CLOSING: return "closing";
        default:      return "closed";
    }
}

static void publish_state(void) { mqtt_publish(DOOR_STATE, state_str(s_state), true); }

static void pulse_relay(void) {
    gpio_set_level(s_relay_pin, s_relay_active_low ? 0 : 1);
    vTaskDelay(pdMS_TO_TICKS(s_pulse_ms));
    gpio_set_level(s_relay_pin, s_relay_active_low ? 1 : 0);
}

static void trigger_open(void) {
    if (s_state == OPEN || s_state == OPENING) return;
    pulse_relay();
    s_state = OPENING;
    s_move_started_ms = now_ms();
    publish_state();
}

static void trigger_close(void) {
    if (s_state == CLOSED || s_state == CLOSING) return;
    pulse_relay();
    s_state = CLOSING;
    s_move_started_ms = now_ms();
    publish_state();
}

static void on_message(const char *topic, int topic_len, const char *data, int data_len) {
    if ((int)strlen(DOOR_SET) != topic_len || strncmp(DOOR_SET, topic, topic_len) != 0) return;
    if (data_len == 4 && strncmp(data, "OPEN", 4) == 0)       trigger_open();
    else if (data_len == 5 && strncmp(data, "CLOSE", 5) == 0) trigger_close();
    else if (data_len == 4 && strncmp(data, "STOP", 4) == 0)  pulse_relay();
}

static void on_connect(void) {
    mqtt_subscribe(DOOR_SET);
    char extra[512];
    snprintf(extra, sizeof(extra),
             "\"command_topic\":\"%s\",\"state_topic\":\"%s\","
             "\"device_class\":\"garage\","
             "\"payload_open\":\"OPEN\",\"payload_close\":\"CLOSE\",\"payload_stop\":\"STOP\","
             "\"state_open\":\"open\",\"state_closed\":\"closed\","
             "\"state_opening\":\"opening\",\"state_closing\":\"closing\"",
             DOOR_SET, DOOR_STATE);
    ha_publish_config("cover", "door", extra);
    publish_state();
}

static void update_task(void *ctx) {
    (void)ctx;
    switch (s_state) {
        case OPENING:
            if (!reed_closed() && (now_ms() - s_move_started_ms) > s_travel_ms) {
                s_state = OPEN; publish_state();
            }
            break;
        case CLOSING:
            if (reed_closed()) { s_state = CLOSED; publish_state(); }
            else if ((now_ms() - s_move_started_ms) > s_travel_ms) { s_state = OPEN; publish_state(); }
            break;
        default:
            if (reed_closed() && s_state != CLOSED)  { s_state = CLOSED; publish_state(); }
            if (!reed_closed() && s_state == CLOSED) { s_state = OPEN;   publish_state(); }
            break;
    }
}

void garage_start(uint8_t relay_pin, uint8_t reed_pin, bool relay_active_low,
                  uint32_t pulse_ms, uint32_t travel_ms) {
    s_relay_pin = relay_pin;
    s_reed_pin = reed_pin;
    s_relay_active_low = relay_active_low;
    s_pulse_ms = pulse_ms;
    s_travel_ms = travel_ms;

    gpio_reset_pin(relay_pin);
    gpio_set_direction(relay_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(relay_pin, relay_active_low ? 1 : 0);

    gpio_reset_pin(reed_pin);
    gpio_set_direction(reed_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(reed_pin, GPIO_PULLUP_ONLY);

    s_state = reed_closed() ? CLOSED : OPEN;

    wifi_sta_start(HOSTNAME);
    ha_set_device(BOARD, "ESP3 Garage");
    mqtt_start(BOARD, on_message, on_connect);

    rtos_every_ms("door", update_task, NULL, 200,
                  RTOS_STACK_DEFAULT, RTOS_PRIO_HIGH, RTOS_CORE_APP);
}
