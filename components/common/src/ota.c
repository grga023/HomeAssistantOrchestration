#include "ota.h"
#include "mqtt_wrap.h"
#include "ha_discovery.h"
#include "app_rtos.h"

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Base URL for per-board release manifests; set via a build flag, e.g.
 * -DOTA_MANIFEST_URL_BASE="\"https://github.com/user/repo/releases/latest/download\"".
 * The manifest for a board lives at "<base>/<board>.json". */
#ifndef OTA_MANIFEST_URL_BASE
#define OTA_MANIFEST_URL_BASE "https://example.invalid"
#endif

static const char *TAG = "ota";

static char s_board[24];
static char s_set_topic[64];      /* home/<board>/ota/set     (direct .bin URL) */
static char s_install_topic[64];  /* home/<board>/ota/install (HA Install button) */
static char s_state_topic[64];    /* home/<board>/ota/state   (JSON) */

static char s_latest_url[256];
static char s_latest_ver[32];

/* Currently running firmware version. */
static const char *installed_version(void) {
    const esp_app_desc_t *desc = esp_app_get_description();
    return desc ? desc->version : "unknown";
}

static void publish_state(void) {
    const char *ver = installed_version();
    const char *latest = s_latest_ver[0] ? s_latest_ver : ver;
    char json[128];
    snprintf(json, sizeof(json),
             "{\"installed_version\":\"%s\",\"latest_version\":\"%s\"}",
             ver, latest);
    mqtt_publish(s_state_topic, json, true);
}

/* Publish the current state plus an error field (for a rejected/failed OTA). */
static void publish_error(const char *err) {
    char json[192];
    snprintf(json, sizeof(json),
             "{\"installed_version\":\"%s\",\"latest_version\":\"%s\","
             "\"error\":\"%s\"}",
             installed_version(),
             s_latest_ver[0] ? s_latest_ver : installed_version(),
             err);
    mqtt_publish(s_state_topic, json, true);
}

/* Allowlist: firmware may only be fetched from the trusted release download
 * base (OTA_MANIFEST_URL_BASE), i.e. the same GitHub host/path the signed
 * manifest lives under. Any other URL is rejected. */
static bool url_is_trusted(const char *url) {
    size_t base_len = strlen(OTA_MANIFEST_URL_BASE);
    return strncmp(url, OTA_MANIFEST_URL_BASE, base_len) == 0;
}

/* GET the per-board manifest and learn the latest version + firmware URL. */
static void fetch_manifest(void) {
    char url[256];
    snprintf(url, sizeof(url), "%s/%s.json", OTA_MANIFEST_URL_BASE, s_board);

    esp_http_client_config_t http = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http);
    if (!client) {
        ESP_LOGE(TAG, "manifest: client init failed");
        goto fail;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "manifest: open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        goto fail;
    }

    esp_http_client_fetch_headers(client);

    char body[512];
    int len = esp_http_client_read_response(client, body, sizeof(body) - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (len <= 0) {
        ESP_LOGE(TAG, "manifest: empty body");
        goto fail;
    }
    body[len] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGE(TAG, "manifest: parse failed");
        goto fail;
    }

    const cJSON *ver = cJSON_GetObjectItem(root, "version");
    const cJSON *dl  = cJSON_GetObjectItem(root, "url");
    if (cJSON_IsString(ver) && ver->valuestring) {
        strlcpy(s_latest_ver, ver->valuestring, sizeof(s_latest_ver));
    }
    if (cJSON_IsString(dl) && dl->valuestring) {
        strlcpy(s_latest_url, dl->valuestring, sizeof(s_latest_url));
    }
    cJSON_Delete(root);

    ESP_LOGI(TAG, "manifest: latest=%s url=%s",
             s_latest_ver[0] ? s_latest_ver : "?",
             s_latest_url[0] ? s_latest_url : "?");
    return;

fail:
    /* Fall back to "up to date" so HA doesn't show a phantom update. */
    strlcpy(s_latest_ver, installed_version(), sizeof(s_latest_ver));
    s_latest_url[0] = '\0';
}

/* One-shot task: download + apply firmware, then reboot on success. */
static void ota_task(void *arg) {
    char *url = (char *)arg;
    ESP_LOGI(TAG, "OTA starting: %s", url);

    esp_http_client_config_t http = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota = { .http_config = &http };

    esp_err_t err = esp_https_ota(&ota);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA success, restarting");
        free(url);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
        char json[160];
        snprintf(json, sizeof(json),
                 "{\"installed_version\":\"%s\",\"latest_version\":\"%s\","
                 "\"error\":\"%s\"}",
                 installed_version(),
                 s_latest_ver[0] ? s_latest_ver : installed_version(),
                 esp_err_to_name(err));
        mqtt_publish(s_state_topic, json, true);
    }

    free(url);
    vTaskDelete(NULL);
}

/* Spawn the OTA task with a malloc'd copy of `url` (freed by the task). Never
 * blocks the caller (the MQTT event callback). */
static void start_ota(const char *url) {
    if (!url || !url[0]) {
        ESP_LOGE(TAG, "start_ota: empty url");
        return;
    }
    if (!url_is_trusted(url)) {
        ESP_LOGE(TAG, "start_ota: rejected untrusted url: %s", url);
        publish_error("untrusted url");
        return;
    }
    char *copy = strdup(url);
    if (!copy) {
        ESP_LOGE(TAG, "start_ota: OOM");
        return;
    }
    if (xTaskCreatePinnedToCore(ota_task, "ota", RTOS_STACK_LARGE, copy,
                                RTOS_PRIO_NORMAL, NULL,
                                RTOS_CORE_NET) != pdPASS) {
        ESP_LOGE(TAG, "start_ota: task create failed");
        free(copy);
    }
}

void ota_start(const char *board) {
    strlcpy(s_board, board, sizeof(s_board));
    snprintf(s_set_topic,     sizeof(s_set_topic),     "home/%s/ota/set",     s_board);
    snprintf(s_install_topic, sizeof(s_install_topic), "home/%s/ota/install", s_board);
    snprintf(s_state_topic,   sizeof(s_state_topic),   "home/%s/ota/state",   s_board);
}

void ota_on_connect(void) {
    mqtt_subscribe(s_set_topic);
    mqtt_subscribe(s_install_topic);

    char extra[512];
    snprintf(extra, sizeof(extra),
        "\"command_topic\":\"%s\","
        "\"payload_install\":\"INSTALL\","
        "\"state_topic\":\"%s\","
        "\"value_template\":\"{{ value_json.installed_version }}\","
        "\"latest_version_topic\":\"%s\","
        "\"latest_version_template\":\"{{ value_json.latest_version }}\","
        "\"device_class\":\"firmware\"",
        s_install_topic, s_state_topic, s_state_topic);
    ha_publish_config("update", "firmware", extra);

    fetch_manifest();
    publish_state();
}

bool ota_handle_message(const char *topic, int topic_len,
                        const char *data, int data_len) {
    if ((int)strlen(s_set_topic) == topic_len &&
        strncmp(s_set_topic, topic, topic_len) == 0) {
        char url[256];
        int n = data_len < (int)sizeof(url) - 1 ? data_len : (int)sizeof(url) - 1;
        memcpy(url, data, n);
        url[n] = '\0';
        start_ota(url);
        return true;
    }

    if ((int)strlen(s_install_topic) == topic_len &&
        strncmp(s_install_topic, topic, topic_len) == 0) {
        if (!s_latest_url[0]) {
            ESP_LOGE(TAG, "install: no manifest url");
        } else if (strcmp(s_latest_ver, installed_version()) == 0) {
            /* Only flash on a genuine upgrade; latest == running is a no-op. */
            ESP_LOGI(TAG, "install: already at %s, skipping", installed_version());
        } else {
            start_ota(s_latest_url);
        }
        return true;
    }

    return false;
}

void ota_confirm_running_image(void) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "OTA image confirmed valid");
    }
}
