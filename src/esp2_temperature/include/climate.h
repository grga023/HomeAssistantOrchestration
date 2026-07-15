/* ESP2 climate controller: DHT22 sensor + optional heater relay. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* dht_pin: DHT22 data GPIO, heater_pin: heater relay GPIO.
 * heater_active_low: true for common relay boards.
 * publish_ms: sensor publish interval. */
void climate_start(uint8_t dht_pin, uint8_t heater_pin,
                   bool heater_active_low, uint32_t publish_ms);

#ifdef __cplusplus
}
#endif
