/* ESP4 security controller (focus of this demo): entry-door reed + PIR motion
 * + siren, driven by an arm/disarm state machine and exposed to Home Assistant
 * as an `alarm_control_panel` plus door/motion binary sensors. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* door_pin: entry reed (to GND), pir_pin: PIR motion (HIGH = motion),
 * siren_pin: siren/buzzer relay. siren_active_low: true for common boards.
 * entry_delay_ms: grace to disarm on entry, exit_delay_ms: grace after arming. */
void security_start(uint8_t door_pin, uint8_t pir_pin, uint8_t siren_pin,
                    bool siren_active_low,
                    uint32_t entry_delay_ms, uint32_t exit_delay_ms);

#ifdef __cplusplus
}
#endif
