/* ESP2 - ESP-WROVER-KIT V4.1: four simulated temperature sensors published
 * over MQTT every 30 s and shown on the on-board LCD. */
#include "climate.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    climate_start(/*publish_ms=*/30000);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
