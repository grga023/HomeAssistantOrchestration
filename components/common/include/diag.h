/* Runtime diagnostics shared by all boards (ESP-IDF).
 *
 * Periodically samples per-core CPU load (from FreeRTOS run-time stats), heap,
 * Wi-Fi RSSI and per-task stack usage, logs them as grep-able "MEAS" CSV rows
 * (same convention as mqtt_wrap.c), and — when MQTT is connected — publishes a
 * retained JSON snapshot to home/<board>/diag/state plus HA Discovery sensors.
 *
 * Wired centrally into mqtt_wrap (like OTA), so every board gets it for free. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Store the board name, build the diag topic and spawn the periodic sampler.
 * Call once from mqtt_start(). */
void diag_start(const char *board);

/* Publish HA Discovery config for the diagnostic sensors (cpu, heap, rssi,
 * uptime). Idempotent — safe to call on every (re)connect. */
void diag_on_connect(void);

#ifdef __cplusplus
}
#endif
