/* See diag.h. Per-core CPU load from FreeRTOS run-time stats + heap/RSSI/tasks,
 * emitted as MEAS lines and published to home/<board>/diag/state. */
#include "diag.h"
#include "mqtt_wrap.h"
#include "ha_discovery.h"
#include "app_rtos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"

#define DIAG_PERIOD_MS 5000
#define DIAG_MAX_TASKS 40

static const char *TAG  = "diag";
static const char *MEAS = "MEAS";          /* shared parse tag (see 04-methodology) */

static char s_board[24];
static char s_state_topic[64];             /* home/<board>/diag/state */

/* previous run-time snapshot, for interval (not since-boot) CPU load */
static uint32_t s_total_rt_prev;           /* monotonic run-time clock (interval denom) */
static uint32_t s_idle_prev[portNUM_PROCESSORS];
static bool     s_have_prev   = false;
static bool     s_tasks_dumped = false;

static int rssi_now(void) {
    wifi_ap_record_t ap;
    return (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
}

/* Per-core idle task is named "IDLE0"/"IDLE1"; a single unsuffixed "IDLE" falls
 * back to its core affinity. Anything else ("IDLExyz") is not a core idle task. */
static int idle_core_of(const TaskStatus_t *t) {
    const char *n = t->pcTaskName;
    if (!(n && n[0] == 'I' && n[1] == 'D' && n[2] == 'L' && n[3] == 'E'))
        return -1;
    if (n[4] == '0') return 0;
    if (n[4] == '1') return 1;
    if (n[4] == '\0' && t->xCoreID >= 0 && t->xCoreID < portNUM_PROCESSORS)
        return t->xCoreID;
    return -1;
}

static void diag_tick(void *ctx) {
    (void)ctx;

    UBaseType_t n = uxTaskGetNumberOfTasks();
    if (n > DIAG_MAX_TASKS) n = DIAG_MAX_TASKS;
    TaskStatus_t *st = calloc(n, sizeof(TaskStatus_t));
    if (!st) return;

    uint32_t total_rt = 0;
    UBaseType_t got = uxTaskGetSystemState(st, n, &total_rt);
    if (got == 0) {          /* over-cap (API is all-or-nothing) or task created mid-call */
        free(st);
        return;
    }

    uint32_t total_now = 0;                     /* Σ task run-time: per-task permille only */
    uint32_t idle_now[portNUM_PROCESSORS] = {0};
    for (UBaseType_t i = 0; i < got; i++) {
        total_now += st[i].ulRunTimeCounter;
        int core = idle_core_of(&st[i]);
        if (core >= 0) idle_now[core] = st[i].ulRunTimeCounter;
    }

    int load[portNUM_PROCESSORS] = {0};
    int load_avg = 0;
    if (s_have_prev) {
        /* per-core capacity over the interval = delta of the MONOTONIC run-time
         * clock (esp_timer). Robust to tasks dying between samples — unlike a Σ
         * of live task counters, which would underflow (e.g. failed OTA task). */
        uint32_t per_core = total_rt - s_total_rt_prev;
        int sum = 0;
        for (int c = 0; c < portNUM_PROCESSORS; c++) {
            uint32_t idle_delta = idle_now[c] - s_idle_prev[c];
            int l = per_core ? (int)(100 - (100ULL * idle_delta) / per_core) : 0;
            if (l < 0)   l = 0;
            if (l > 100) l = 100;
            load[c] = l;
            sum += l;
        }
        load_avg = sum / portNUM_PROCESSORS;
    }
    s_total_rt_prev = total_rt;
    for (int c = 0; c < portNUM_PROCESSORS; c++) s_idle_prev[c] = idle_now[c];
    s_have_prev = true;

    uint32_t heap_free    = esp_get_free_heap_size();
    uint32_t heap_min     = esp_get_minimum_free_heap_size();
    uint32_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    int      rssi         = rssi_now();
    uint32_t uptime_s     = (uint32_t)(esp_timer_get_time() / 1000000);

    /* MEAS rows (aligned CSV, same tag as mqtt_wrap) */
    ESP_LOGI(MEAS, "%s,cpu,load,%d,%d,%d", s_board,
             load[0], (portNUM_PROCESSORS > 1) ? load[1] : 0, load_avg);
    ESP_LOGI(MEAS, "%s,heap,live,%" PRIu32 ",%" PRIu32 ",%" PRIu32,
             s_board, heap_free, heap_min, heap_largest);
    ESP_LOGI(MEAS, "%s,wifi,rssi,%d,,", s_board, rssi);
    ESP_LOGI(MEAS, "%s,rtos,tasks,%u,,", s_board, (unsigned)got);

    /* one-time per-task table: stack head-room (words) + cumulative CPU permille */
    if (!s_tasks_dumped && total_now) {
        for (UBaseType_t i = 0; i < got; i++) {
            unsigned permille = (unsigned)((1000ULL * st[i].ulRunTimeCounter) / total_now);
            ESP_LOGI(MEAS, "%s,task,%s,%u,%u,", s_board,
                     st[i].pcTaskName ? st[i].pcTaskName : "?",
                     (unsigned)st[i].usStackHighWaterMark, permille);
        }
        s_tasks_dumped = true;
    }

    free(st);

    if (mqtt_is_connected()) {
        char json[224];
        snprintf(json, sizeof(json),
            "{\"cpu0\":%d,\"cpu1\":%d,\"cpu_avg\":%d,"
            "\"heap_free\":%" PRIu32 ",\"heap_min\":%" PRIu32 ",\"heap_largest\":%" PRIu32 ","
            "\"rssi\":%d,\"uptime_s\":%" PRIu32 ",\"tasks\":%u}",
            load[0], (portNUM_PROCESSORS > 1) ? load[1] : 0, load_avg,
            heap_free, heap_min, heap_largest, rssi, uptime_s, (unsigned)got);
        mqtt_publish(s_state_topic, json, true);
    }
}

void diag_start(const char *board) {
    strlcpy(s_board, board, sizeof s_board);
    snprintf(s_state_topic, sizeof s_state_topic, "home/%s/diag/state", board);
    /* low priority on the networking core; drift-free periodic sampler */
    rtos_every_ms("diag", diag_tick, NULL, DIAG_PERIOD_MS,
                  RTOS_STACK_DEFAULT, RTOS_PRIO_LOW, RTOS_CORE_NET);
    ESP_LOGI(TAG, "sampling every %d ms (core %d)", DIAG_PERIOD_MS, RTOS_CORE_NET);
}

void diag_on_connect(void) {
    static const struct { const char *id; const char *extra; } sensors[] = {
        { "cpu",
          "\"unit_of_measurement\":\"%\",\"icon\":\"mdi:cpu-32-bit\","
          "\"value_template\":\"{{ value_json.cpu_avg }}\"" },
        { "heap_free",
          "\"unit_of_measurement\":\"B\",\"icon\":\"mdi:memory\","
          "\"value_template\":\"{{ value_json.heap_free }}\"" },
        { "rssi",
          "\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\","
          "\"entity_category\":\"diagnostic\","
          "\"value_template\":\"{{ value_json.rssi }}\"" },
        { "uptime",
          "\"unit_of_measurement\":\"s\",\"device_class\":\"duration\","
          "\"entity_category\":\"diagnostic\","
          "\"value_template\":\"{{ value_json.uptime_s }}\"" },
    };
    char extra[320];
    for (size_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); i++) {
        snprintf(extra, sizeof extra, "\"state_topic\":\"%s\",%s",
                 s_state_topic, sensors[i].extra);
        ha_publish_config("sensor", sensors[i].id, extra);
    }
}
