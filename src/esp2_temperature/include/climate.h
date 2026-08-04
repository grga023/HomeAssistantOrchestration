/* ESP2 (ESP-WROVER-KIT V4.1): 4 simulated temperature sensors published over
 * MQTT and shown on the on-board LCD. */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up the LCD, Wi-Fi, MQTT + HA discovery, then publish 4 dummy
 * temperatures and refresh the screen every publish_ms milliseconds. */
void climate_start(uint32_t publish_ms);

#ifdef __cplusplus
}
#endif
