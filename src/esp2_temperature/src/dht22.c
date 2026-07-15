#include "dht22.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

/* Busy-wait until `pin` reaches `level`, up to `timeout_us`.
 * Returns the microseconds waited, or -1 on timeout. */
static int wait_level(gpio_num_t pin, int level, int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) != level) {
        if ((esp_timer_get_time() - start) > timeout_us) return -1;
    }
    return (int)(esp_timer_get_time() - start);
}

esp_err_t dht22_read(gpio_num_t pin, float *temperature, float *humidity) {
    uint8_t bytes[5] = {0};

    /* --- MCU start signal: pull low >=1ms, then release. --- */
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(1200);
    gpio_set_level(pin, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    /* --- Sensor response: ~80us low, ~80us high. --- */
    if (wait_level(pin, 0, 100) < 0) return ESP_ERR_TIMEOUT;
    if (wait_level(pin, 1, 100) < 0) return ESP_ERR_TIMEOUT;
    if (wait_level(pin, 0, 100) < 0) return ESP_ERR_TIMEOUT;

    /* --- 40 data bits. Each: ~50us low, then high 26-28us (0) or ~70us (1). --- */
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);
    esp_err_t err = ESP_OK;
    for (int i = 0; i < 40; i++) {
        if (wait_level(pin, 1, 80) < 0) { err = ESP_ERR_TIMEOUT; break; }
        int high_us = wait_level(pin, 0, 100);
        if (high_us < 0) { err = ESP_ERR_TIMEOUT; break; }
        bytes[i / 8] <<= 1;
        if (high_us > 45) bytes[i / 8] |= 1;  /* long high pulse => '1' */
    }
    portEXIT_CRITICAL(&mux);
    if (err != ESP_OK) return err;

    /* --- Checksum. --- */
    uint8_t sum = bytes[0] + bytes[1] + bytes[2] + bytes[3];
    if (sum != bytes[4]) return ESP_ERR_INVALID_CRC;

    uint16_t raw_h = ((uint16_t)bytes[0] << 8) | bytes[1];
    uint16_t raw_t = ((uint16_t)(bytes[2] & 0x7F) << 8) | bytes[3];
    *humidity = raw_h * 0.1f;
    *temperature = raw_t * 0.1f;
    if (bytes[2] & 0x80) *temperature = -*temperature;
    return ESP_OK;
}
