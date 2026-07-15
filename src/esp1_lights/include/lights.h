/* ESP1 light controller: drives a multi-channel relay board and exposes each
 * channel to Home Assistant as a switch. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configure relays, bring up WiFi + MQTT, and start serving commands.
 * pins: relay GPIOs, count: how many, active_low: true for common boards. */
void lights_start(const uint8_t *pins, size_t count, bool active_low);

#ifdef __cplusplus
}
#endif
