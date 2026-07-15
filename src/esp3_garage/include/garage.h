/* ESP3 garage door controller: relay pulse to trigger the opener + reed switch
 * for closed-state. Exposed to Home Assistant as a `cover`. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* relay_pin: pulses the opener, reed_pin: closed-door switch (to GND).
 * relay_active_low: true for common relay boards.
 * pulse_ms: opener button press length, travel_ms: full open/close time. */
void garage_start(uint8_t relay_pin, uint8_t reed_pin, bool relay_active_low,
                  uint32_t pulse_ms, uint32_t travel_ms);

#ifdef __cplusplus
}
#endif
