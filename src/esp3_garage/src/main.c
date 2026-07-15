/* ESP3 - Garage door controller. */
#include "garage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RELAY_PIN 23   /* pulses the garage opener */
#define REED_PIN  18   /* closed-door reed switch (to GND) */

void app_main(void) {
    garage_start(RELAY_PIN, REED_PIN, /*relay_active_low=*/true,
                 /*pulse_ms=*/500, /*travel_ms=*/15000);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
