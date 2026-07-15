/* ESP4 - Security controller. */
#include "security.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DOOR_PIN  18   /* entry-door reed switch (to GND) */
#define PIR_PIN   19   /* PIR motion sensor (HIGH = motion) */
#define SIREN_PIN 23   /* siren / buzzer relay */

void app_main(void) {
    security_start(DOOR_PIN, PIR_PIN, SIREN_PIN, /*siren_active_low=*/true,
                   /*entry_delay_ms=*/20000, /*exit_delay_ms=*/15000);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
