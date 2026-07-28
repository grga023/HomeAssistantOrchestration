#include "mqtt_wrap.h"
#include "secrets.h"
#include "ota.h"

#include <string.h>
#include <stdio.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include <inttypes.h>
#include "sdkconfig.h"

#if defined(MQTT_USE_TLS) && !defined(MQTT_URI_TLS)
#  error "esp2 TLS build requires MQTT_URI_TLS in secrets.h"
#endif

#ifdef MQTT_USE_TLS
extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");
#endif

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_client;
static mqtt_msg_cb   s_on_msg;
static mqtt_conn_cb  s_on_conn;
static char          s_avail_topic[64];
static volatile bool s_connected = false;

/* MEAS: instrumentation for the TLS-overhead measurements (see 04-methodology).
 * All output goes through a single grep-able "MEAS" tag as one CSV row, so the
 * columns line up byte-for-byte between esp1 (plain) and esp2 (TLS). */
static const char *MEAS = "MEAS";
static char     s_board[24];
static int64_t  s_t_start_us, s_t_before_us;
static uint32_t s_heap_pre_start;
static bool     s_lowwater_armed = false;

static void meas_heap(const char *phase) {
    ESP_LOGI(MEAS, "%s,heap,%s,%" PRIu32 ",%u,%u",
        s_board, phase,
        esp_get_free_heap_size(),                                               /* a: total free (=ESP.getFreeHeap) */
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT), /* b: internal DRAM */
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));       /* c: largest block (fragmentation) */
}

static void meas_time(const char *phase, int64_t dt_us) {
    ESP_LOGI(MEAS, "%s,time,%s,%lld,%lld,", s_board, phase, dt_us, dt_us/1000);
}

static void meas_tlscfg(void) {
#ifdef MQTT_USE_TLS
  #ifdef CONFIG_MBEDTLS_DYNAMIC_BUFFER
    int dynbuf = 1;
  #else
    int dynbuf = 0;   /* macro is UNDEFINED when off — must #ifdef, never read as value */
  #endif
    ESP_LOGI(MEAS, "%s,tlscfg,in_out_dyn,%d,%d,%d",
        s_board, CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN,
        CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN, dynbuf);
#else
    ESP_LOGI(MEAS, "%s,tlscfg,plain,0,0,0", s_board);
#endif
}

static void lowwater_cb(void *arg) {   /* 5 s after connect */
    ESP_LOGI(MEAS, "%s,heap,low_water_5s,%" PRIu32 ",,", s_board,
             esp_get_minimum_free_heap_size());
}

static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        /* MEAS: start of the (non-blocking) connect on the esp-mqtt task. */
        s_t_before_us = esp_timer_get_time();
        meas_time("start_to_before", s_t_before_us - s_t_start_us);
        break;
    case MQTT_EVENT_CONNECTED: {
        s_connected = true;
        /* MEAS: handshake span = TCP + TLS handshake + CONNECT/CONNACK. */
        int64_t now = esp_timer_get_time();
        meas_time("connect_span",  now - s_t_before_us);
        meas_time("connect_total", now - s_t_start_us);
        meas_heap("connected");
        ESP_LOGI(MEAS, "%s,heapdelta,retained,%d,,",
                 s_board, (int)s_heap_pre_start - (int)esp_get_free_heap_size());
        if (!s_lowwater_armed) {        /* one-shot — CONNECTED repeats on every reconnect */
            const esp_timer_create_args_t a = { .callback = lowwater_cb, .name = "lw" };
            esp_timer_handle_t h; esp_timer_create(&a, &h);
            esp_timer_start_once(h, 5 * 1000 * 1000);
            s_lowwater_armed = true;
        }
        ESP_LOGI(TAG, "connected");
        /* Announce availability (retained), then let the board (re)subscribe. */
        esp_mqtt_client_publish(s_client, s_avail_topic, "online", 0, 0, 1);
        if (s_on_conn) s_on_conn();
        /* OTA: a successful connect proves this image works, so confirm it
         * (cancels the pending rollback); then advertise the update entity. */
        ota_confirm_running_image();
        ota_on_connect();
    } break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        /* OTA command topics are handled centrally for every board; anything
         * else is handed off to the board's own message callback. */
        if (ota_handle_message(event->topic, event->topic_len,
                               event->data, event->data_len)) {
            break;
        }
        if (s_on_msg) {
            s_on_msg(event->topic, event->topic_len, event->data, event->data_len);
        }
        break;
    default:
        break;
    }
}

void mqtt_start(const char *board, mqtt_msg_cb on_msg, mqtt_conn_cb on_conn) {
    s_on_msg = on_msg;
    s_on_conn = on_conn;
    snprintf(s_avail_topic, sizeof(s_avail_topic), "home/%s/status", board);
    ota_start(board);

    /* MEAS: post-WiFi baseline, same starting point on both nodes. */
    strlcpy(s_board, board, sizeof s_board);
    meas_tlscfg();
    meas_heap("entry");

    esp_mqtt_client_config_t cfg = {
#ifdef MQTT_USE_TLS
        .broker.address.uri = MQTT_URI_TLS,
#else
        .broker.address.uri = MQTT_URI,
#endif
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASSWORD,
        /* Last-Will: broker publishes "offline" (retained) if we drop. */
        .session.last_will.topic = s_avail_topic,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 7,
        .session.last_will.qos = 0,
        .session.last_will.retain = 1,
    };

#ifdef MQTT_USE_TLS
    cfg.broker.verification.certificate = (const char *)ca_crt_start; /* NUL-terminated PEM */
    /* skip=true: the chain/signature are STILL verified against our CA;
       only the name/IP match is skipped. Flip to false for a "full
       verification" run (DNS-SAN homeassistant.local). See 03. */
    cfg.broker.verification.skip_cert_common_name_check = true;
#endif

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    s_heap_pre_start = esp_get_free_heap_size();
    meas_heap("pre_start");            /* immediately before start */
    s_t_start_us = esp_timer_get_time();
    esp_mqtt_client_start(s_client);   /* NON-BLOCKING — do not measure around this */
}

int mqtt_publish(const char *topic, const char *payload, bool retain) {
    return esp_mqtt_client_publish(s_client, topic, payload, 0, 0, retain ? 1 : 0);
}

int mqtt_subscribe(const char *topic) {
    /* QoS 1: commands must survive a single dropped packet (broker redelivers). */
    return esp_mqtt_client_subscribe(s_client, topic, 1);
}

bool mqtt_is_connected(void) {
    return s_connected;
}

const char *mqtt_availability_topic(void) {
    return s_avail_topic;
}
