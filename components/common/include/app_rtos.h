/* Small FreeRTOS helper shared by all boards: run a function periodically on
 * its own task with accurate (drift-free) timing. FreeRTOS is native to
 * ESP-IDF, so this just centralises our task conventions. */
#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Core assignment: keep app logic off core 0's networking housekeeping. */
#define RTOS_CORE_APP     1
#define RTOS_CORE_NET     0

#define RTOS_STACK_SMALL   2048
#define RTOS_STACK_DEFAULT 4096
#define RTOS_STACK_LARGE   8192

#define RTOS_PRIO_LOW      1
#define RTOS_PRIO_NORMAL   2
#define RTOS_PRIO_HIGH     3   /* security-critical (alarm, door) */

typedef void (*rtos_periodic_fn)(void *ctx);

/* Spawn a task that calls fn(ctx) every period_ms forever. */
TaskHandle_t rtos_every_ms(const char *name, rtos_periodic_fn fn, void *ctx,
                           uint32_t period_ms, uint32_t stack,
                           UBaseType_t prio, BaseType_t core);

#ifdef __cplusplus
}
#endif
