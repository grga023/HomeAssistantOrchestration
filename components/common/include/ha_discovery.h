/* Home Assistant MQTT Discovery helpers (ESP-IDF, C).
 * Publishes retained config topics so entities auto-appear in HA.
 * https://www.home-assistant.io/integrations/mqtt/#discovery */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Set the device identity that groups a board's entities in HA. Call once. */
void ha_set_device(const char *board, const char *display_name);

/* Publish one entity's discovery config.
 *   component  : "switch", "sensor", "binary_sensor", "cover",
 *                "alarm_control_panel", ...
 *   object_id  : unique per board, e.g. "light1".
 *   extra_json : component-specific keys WITHOUT braces, e.g.
 *                "\"command_topic\":\"home/lights/light1/set\","
 *                "\"state_topic\":\"home/lights/light1/state\""
 * Availability + device block are added automatically. */
void ha_publish_config(const char *component, const char *object_id,
                       const char *extra_json);

#ifdef __cplusplus
}
#endif
