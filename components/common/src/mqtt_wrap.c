#include "mqtt_wrap.h"
#include "secrets.h"

#include <string.h>
#include <stdio.h>
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_client;
static mqtt_msg_cb   s_on_msg;
static mqtt_conn_cb  s_on_conn;
static char          s_avail_topic[64];
static volatile bool s_connected = false;

static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        /* Announce availability (retained), then let the board (re)subscribe. */
        esp_mqtt_client_publish(s_client, s_avail_topic, "online", 0, 0, 1);
        if (s_on_conn) s_on_conn();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        if (s_on_msg) {
            s_on_msg(event->topic, event->topic_len, event->data, event->data_len);
        }
        break;
    default:
        break;
    }
}

void mqtt_start(const char *board, mqtt_msg_cb on_msg, mqtt_conn_cb on_conn) {
    s_on_msg = on_msg;
    s_on_conn = on_conn;
    snprintf(s_avail_topic, sizeof(s_avail_topic), "home/%s/status", board);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASSWORD,
        /* Last-Will: broker publishes "offline" (retained) if we drop. */
        .session.last_will.topic = s_avail_topic,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 7,
        .session.last_will.qos = 0,
        .session.last_will.retain = 1,
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

int mqtt_publish(const char *topic, const char *payload, bool retain) {
    return esp_mqtt_client_publish(s_client, topic, payload, 0, 0, retain ? 1 : 0);
}

int mqtt_subscribe(const char *topic) {
    return esp_mqtt_client_subscribe(s_client, topic, 0);
}

bool mqtt_is_connected(void) {
    return s_connected;
}

const char *mqtt_availability_topic(void) {
    return s_avail_topic;
}
