/* MQTT client wrapper shared by all boards (ESP-IDF esp-mqtt).
 * Handles connect with Last-Will (availability), (re)subscribe on connect,
 * and dispatches incoming messages to a user callback. */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Incoming message: topic/data are NOT null-terminated, use the lengths. */
typedef void (*mqtt_msg_cb)(const char *topic, int topic_len,
                            const char *data, int data_len);

/* Called on every (re)connect so a board can subscribe + publish discovery. */
typedef void (*mqtt_conn_cb)(void);

/* Start MQTT for this board. `board` (e.g. "security") builds the availability
 * topic "home/<board>/status" published via Last-Will. */
void mqtt_start(const char *board, mqtt_msg_cb on_msg, mqtt_conn_cb on_conn);

int  mqtt_publish(const char *topic, const char *payload, bool retain);
int  mqtt_subscribe(const char *topic);
bool mqtt_is_connected(void);

/* "home/<board>/status" */
const char *mqtt_availability_topic(void);

#ifdef __cplusplus
}
#endif
