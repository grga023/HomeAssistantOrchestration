#include "security.h"
#include "wifi_sta.h"
#include "mqtt_wrap.h"
#include "ha_discovery.h"
#include "app_rtos.h"

#include <string.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#define BOARD    "security"
#define HOSTNAME "esp32-security"

static const char *TAG = "security";
static const char *ALARM_SET    = "home/security/alarm/set";
static const char *ALARM_STATE  = "home/security/alarm/state";
static const char *DOOR_STATE   = "home/security/door";
static const char *MOTION_STATE = "home/security/motion";

/* Matches Home Assistant alarm_control_panel states. */
typedef enum { DISARMED, ARMED_HOME, ARMED_AWAY, PENDING, TRIGGERED, ARMING } alarm_t;

static uint8_t s_door_pin, s_pir_pin, s_siren_pin;
static bool s_siren_active_low;
static uint32_t s_entry_delay_ms, s_exit_delay_ms;

static alarm_t s_state = DISARMED;
static alarm_t s_arm_target = DISARMED;
static int64_t s_phase_start_ms;
static int s_last_door = -1;
static int s_last_motion = -1;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static bool door_open(void) { return gpio_get_level(s_door_pin) == 1; }  /* active-LOW closed */
static bool motion(void)    { return gpio_get_level(s_pir_pin) == 1; }

static const char *alarm_str(alarm_t a) {
    switch (a) {
        case ARMED_HOME: return "armed_home";
        case ARMED_AWAY: return "armed_away";
        case PENDING:    return "pending";
        case TRIGGERED:  return "triggered";
        case ARMING:     return "arming";
        default:         return "disarmed";
    }
}

static void set_siren(bool on) {
    gpio_set_level(s_siren_pin, (on ^ s_siren_active_low) ? 1 : 0);
}

static void publish_alarm(void) { mqtt_publish(ALARM_STATE, alarm_str(s_state), true); }

static void transition(alarm_t next) {
    s_state = next;
    s_phase_start_ms = now_ms();
    set_siren(next == TRIGGERED);
    publish_alarm();
    ESP_LOGI(TAG, "-> %s", alarm_str(next));
}

static void arm(alarm_t target) {
    s_arm_target = target;
    /* ARMED_HOME arms instantly (you're inside); AWAY gives an exit delay. */
    if (target == ARMED_HOME) transition(ARMED_HOME);
    else transition(ARMING);
}

static void on_message(const char *topic, int topic_len, const char *data, int data_len) {
    if ((int)strlen(ALARM_SET) != topic_len || strncmp(ALARM_SET, topic, topic_len) != 0) return;
    if (data_len == 6 && strncmp(data, "DISARM", 6) == 0)        transition(DISARMED);
    else if (data_len == 8 && strncmp(data, "ARM_HOME", 8) == 0) arm(ARMED_HOME);
    else if (data_len == 8 && strncmp(data, "ARM_AWAY", 8) == 0) arm(ARMED_AWAY);
}

static void on_connect(void) {
    mqtt_subscribe(ALARM_SET);

    char extra[512];
    snprintf(extra, sizeof(extra),
             "\"command_topic\":\"%s\",\"state_topic\":\"%s\","
             "\"supported_features\":[\"arm_home\",\"arm_away\"],"
             "\"code_arm_required\":false,\"code_disarm_required\":false",
             ALARM_SET, ALARM_STATE);
    ha_publish_config("alarm_control_panel", "alarm", extra);

    snprintf(extra, sizeof(extra),
             "\"state_topic\":\"%s\",\"device_class\":\"door\","
             "\"payload_on\":\"open\",\"payload_off\":\"closed\"", DOOR_STATE);
    ha_publish_config("binary_sensor", "door", extra);

    snprintf(extra, sizeof(extra),
             "\"state_topic\":\"%s\",\"device_class\":\"motion\","
             "\"payload_on\":\"detected\",\"payload_off\":\"clear\"", MOTION_STATE);
    ha_publish_config("binary_sensor", "motion", extra);

    publish_alarm();
}

static void tick_task(void *ctx) {
    (void)ctx;
    bool d = door_open(), m = motion();

    if ((int)d != s_last_door)   { s_last_door = d;   mqtt_publish(DOOR_STATE,   d ? "open" : "closed", true); }
    if ((int)m != s_last_motion) { s_last_motion = m; mqtt_publish(MOTION_STATE, m ? "detected" : "clear", true); }

    int64_t elapsed = now_ms() - s_phase_start_ms;
    switch (s_state) {
        case ARMING:
            if (elapsed >= s_exit_delay_ms) transition(s_arm_target);  /* -> ARMED_AWAY */
            break;
        case ARMED_AWAY:
            if (d || m) { s_arm_target = ARMED_AWAY; transition(PENDING); }
            break;
        case ARMED_HOME:
            /* Perimeter only: door triggers, interior motion is ignored. */
            if (d) { s_arm_target = ARMED_HOME; transition(PENDING); }
            break;
        case PENDING:
            if (elapsed >= s_entry_delay_ms) transition(TRIGGERED);
            break;
        default:
            break;
    }
}

void security_start(uint8_t door_pin, uint8_t pir_pin, uint8_t siren_pin,
                    bool siren_active_low,
                    uint32_t entry_delay_ms, uint32_t exit_delay_ms) {
    s_door_pin = door_pin;
    s_pir_pin = pir_pin;
    s_siren_pin = siren_pin;
    s_siren_active_low = siren_active_low;
    s_entry_delay_ms = entry_delay_ms;
    s_exit_delay_ms = exit_delay_ms;

    gpio_reset_pin(door_pin);
    gpio_set_direction(door_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(door_pin, GPIO_PULLUP_ONLY);

    gpio_reset_pin(pir_pin);
    gpio_set_direction(pir_pin, GPIO_MODE_INPUT);

    gpio_reset_pin(siren_pin);
    gpio_set_direction(siren_pin, GPIO_MODE_OUTPUT);
    set_siren(false);

    wifi_sta_start(HOSTNAME);
    ha_set_device(BOARD, "ESP4 Security");
    mqtt_start(BOARD, on_message, on_connect);

    /* Security logic runs at high priority on its own task. */
    rtos_every_ms("security", tick_task, NULL, 100,
                  RTOS_STACK_DEFAULT, RTOS_PRIO_HIGH, RTOS_CORE_APP);
}
