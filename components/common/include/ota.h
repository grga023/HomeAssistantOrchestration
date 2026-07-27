/* Over-the-air (OTA) firmware update, triggered over MQTT.
 * Firmware is downloaded over HTTPS from GitHub Releases and verified with the
 * ESP-IDF public certificate bundle (esp_crt_bundle). Exposes a Home Assistant
 * "update" entity so a new release can be installed from the HA dashboard. */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Store the board id and build the OTA topics. Call once from mqtt_start. */
void ota_start(const char *board);

/* (Re)subscribe to OTA command topics, publish the HA update-entity discovery,
 * fetch the release manifest, and publish current state. Call on every MQTT
 * (re)connect. */
void ota_on_connect(void);

/* Dispatch an incoming MQTT message. Returns true if it consumed an OTA topic
 * (topic/data are NOT null-terminated, use the lengths). */
bool ota_handle_message(const char *topic, int topic_len,
                        const char *data, int data_len);

/* If the running image is still PENDING_VERIFY, mark it valid to cancel the
 * rollback. Call after a successful MQTT connect (i.e. once we know we work). */
void ota_confirm_running_image(void);

#ifdef __cplusplus
}
#endif
