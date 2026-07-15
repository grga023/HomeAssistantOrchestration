/* ESP1 - Light control via a multi-channel relay board. */
#include "lights.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Relay channels (active-LOW modules are common). Adjust to your wiring. */
static const uint8_t RELAY_PINS[] = {23, 22, 21, 19};

void app_main(void) {
    lights_start(RELAY_PINS, sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]), true);

    /* Everything runs on WiFi/MQTT event tasks; idle here. */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
