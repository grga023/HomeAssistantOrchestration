#include "app_rtos.h"
#include <stdlib.h>

typedef struct {
    rtos_periodic_fn fn;
    void *ctx;
    uint32_t period_ms;
} periodic_args_t;

static void periodic_trampoline(void *raw) {
    periodic_args_t args = *(periodic_args_t *)raw;
    free(raw);

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(args.period_ms);
    for (;;) {
        args.fn(args.ctx);
        vTaskDelayUntil(&last_wake, period);
    }
}

TaskHandle_t rtos_every_ms(const char *name, rtos_periodic_fn fn, void *ctx,
                           uint32_t period_ms, uint32_t stack,
                           UBaseType_t prio, BaseType_t core) {
    periodic_args_t *args = malloc(sizeof(periodic_args_t));
    args->fn = fn;
    args->ctx = ctx;
    args->period_ms = period_ms;

    TaskHandle_t handle = NULL;
    xTaskCreatePinnedToCore(periodic_trampoline, name, stack, args, prio, &handle, core);
    return handle;
}
