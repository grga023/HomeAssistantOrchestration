/* WiFi station bring-up shared by all boards (ESP-IDF). */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise NVS + netif + event loop and connect to the AP from secrets.h.
 * Sets the given hostname. Blocks until connected or a few retries fail. */
void wifi_sta_start(const char *hostname);

bool wifi_sta_connected(void);

#ifdef __cplusplus
}
#endif
