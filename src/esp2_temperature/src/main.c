/* ESP2 - Temperature & humidity (DHT22) with an optional heater relay. */
#include "climate.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DHT_PIN     4
#define HEATER_PIN  23

void app_main(void) {
    climate_start(DHT_PIN, HEATER_PIN, /*heater_active_low=*/true, /*publish_ms=*/15000);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
