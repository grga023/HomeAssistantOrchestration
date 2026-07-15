/* Copy this file to "secrets.h" (same folder) and fill in your values.
 * secrets.h is gitignored so your credentials stay out of the repo. */
#pragma once

/* ---- WiFi ---- */
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

/* ---- MQTT broker (Mosquitto on the Raspberry Pi / Home Assistant) ---- */
#define MQTT_URI        "mqtt://192.168.1.10:1883"  /* Raspberry Pi broker */
#define MQTT_USER       "mqttuser"                  /* "" if anonymous */
#define MQTT_PASSWORD   "mqttpass"
