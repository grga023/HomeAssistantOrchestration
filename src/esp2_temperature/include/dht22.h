/* Minimal bit-bang DHT22 (AM2302) driver for ESP-IDF.
 * Single-wire protocol; one sensor per GPIO. Read no faster than ~2 s. */
#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reads temperature (deg C) and humidity (%). Returns ESP_OK on success,
 * ESP_ERR_TIMEOUT on a protocol timeout, ESP_ERR_INVALID_CRC on checksum. */
esp_err_t dht22_read(gpio_num_t pin, float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif
