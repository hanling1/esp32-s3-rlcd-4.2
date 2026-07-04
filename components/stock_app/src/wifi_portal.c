#include "wifi_portal.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dns_server.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "lwip/inet.h"

#define WIFI_PORTAL_MAX_FORM_BODY 512
#define WIFI_PORTAL_MAX_SCAN_AP 20
#define WIFI_PORTAL_JSON_BUF_SIZE 3072
#define WIFI_PORTAL_NVS_NAMESPACE "wifi_portal"
#define WIFI_PORTAL_NVS_KEY_STA_SSID "sta_ssid"
#define WIFI_PORTAL_NVS_KEY_STA_PASS "sta_pass"
#define WIFI_PORTAL_BOOT_RETRY_MAX 3

static const char *TAG = "wifi_portal";

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");

typedef struct {
    bool started;
    bool portal_running;
    bool boot_connect_pending;
    uint8_t boot_connect_retry;
    httpd_handle_t http_server;
    dns_server_handle_t dns_server;
    esp_event_handler_instance_t wifi_handler;
    esp_event_handler_instance_t ip_handler;
    SemaphoreHandle_t lock;
    char ap_ssid_prefix[25];
    char ap_password[65];
    uint8_t ap_channel;
    uint8_t max_connection;
    wifi_portal_state_t state;
} wifi_portal_context_t;

static wifi_portal_context_t s_ctx = {0};

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1U;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void cache_runtime_config(const wifi_portal_config_t *config)
{
    const char *prefix =
        (config->ap_ssid_prefix != NULL && config->ap_ssid_prefix[0] != '\0')
            ? config->ap_ssid_prefix
            : "ESP32_WIFI";

    copy_text(s_ctx.ap_ssid_prefix, sizeof(s_ctx.ap_ssid_prefix), prefix);

    if (config->ap_password != NULL && strlen(config->ap_password) >= 8U) {
        copy_text(s_ctx.ap_password, sizeof(s_ctx.ap_password), config->ap_password);
    } else {
        s_ctx.ap_password[0] = '\0';
    }

    s_ctx.ap_channel = (config->ap_channel == 0U) ? 1U : config->ap_channel;
    s_ctx.max_connection = (config->max_connection == 0U) ? 4U : config->max_connection;
}

static void build_ap_config(wifi_config_t *ap_cfg, char *ap_ssid, size_t ap_ssid_size)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

    snprintf(ap_ssid,
             ap_ssid_size,
             "%s_%02X%02X%02X",
             s_ctx.ap_ssid_prefix,
             mac[3],
             mac[4],
             mac[5]);

    memset(ap_cfg, 0, sizeof(*ap_cfg));
    copy_text((char *)ap_cfg->ap.ssid, sizeof(ap_cfg->ap.ssid), ap_ssid);
    ap_cfg->ap.ssid_len = strlen((const char *)ap_cfg->ap.ssid);
    ap_cfg->ap.channel = s_ctx.ap_channel;
    ap_cfg->ap.max_connection = s_ctx.max_connection;

    if (s_ctx.ap_password[0] != '\0') {
        copy_text((char *)ap_cfg->ap.password,
                  sizeof(ap_cfg->ap.password),
                  s_ctx.ap_password);
        ap_cfg->ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_cfg->ap.password[0] = '\0';
        ap_cfg->ap.authmode = WIFI_AUTH_OPEN;
    }
}

static esp_err_t nvs_load_sta_credentials(char *ssid,
                                          size_t ssid_size,
                                          char *password,
                                          size_t password_size,
                                          bool *found)
{
    if (ssid == NULL || password == NULL || found == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *found = false;
    ssid[0] = '\0';
    password[0] = '\0';

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_PORTAL_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    size_t ssid_len = ssid_size;
    err = nvs_get_str(handle, WIFI_PORTAL_NVS_KEY_STA_SSID, ssid, &ssid_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ESP_OK;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    size_t password_len = password_size;
    err = nvs_get_str(handle, WIFI_PORTAL_NVS_KEY_STA_PASS, password, &password_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        password[0] = '\0';
        err = ESP_OK;
    }

    nvs_close(handle);
    if (err != ESP_OK) {
        return err;
    }

    *found = (ssid[0] != '\0');
    return ESP_OK;
}

static esp_err_t nvs_save_sta_credentials(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0' || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_PORTAL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, WIFI_PORTAL_NVS_KEY_STA_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, WIFI_PORTAL_NVS_KEY_STA_PASS, password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t wifi_portal_erase_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_PORTAL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static const char *disconnect_reason_text(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_AUTH_FAIL:
        return "auth failed";
    case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY:
    case WIFI_REASON_ASSOC_FAIL:
        return "association failed";
    case WIFI_REASON_NO_AP_FOUND:
        return "ssid not found";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "handshake timeout";
    default:
        return "connection failed";
    }
}

const char *wifi_portal_status_string(wifi_portal_connect_status_t status)
{
    switch (status) {
    case WIFI_PORTAL_STATUS_IDLE:
        return "idle";
    case WIFI_PORTAL_STATUS_CONNECTING:
        return "connecting";
    case WIFI_PORTAL_STATUS_CONNECTED:
        return "connected";
    case WIFI_PORTAL_STATUS_FAILED:
        return "failed";
    default:
        return "idle";
    }
}

static void portal_set_state(wifi_portal_connect_status_t status, const char *ip, const char *reason)
{
    if (s_ctx.lock != NULL) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    }

    s_ctx.state.status = status;
    if (ip != NULL) {
        copy_text(s_ctx.state.sta_ip, sizeof(s_ctx.state.sta_ip), ip);
    }
    if (reason != NULL) {
        copy_text(s_ctx.state.fail_reason, sizeof(s_ctx.state.fail_reason), reason);
    }

    if (s_ctx.lock != NULL) {
        xSemaphoreGive(s_ctx.lock);
    }
}

void wifi_portal_get_state(wifi_portal_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (s_ctx.lock != NULL) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    }
    memcpy(state, &s_ctx.state, sizeof(*state));
    if (s_ctx.lock != NULL) {
        xSemaphoreGive(s_ctx.lock);
    }
}

static void url_decode(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_size; ++i) {
        if (src[i] == '%' && src[i + 1] != '\0' && src[i + 2] != '\0') {
            char hex[3] = {src[i + 1], src[i + 2], '\0'};
            dst[out++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[out++] = ' ';
        } else {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
}

static bool get_form_value(const char *body, const char *key, char *value, size_t value_size)
{
    size_t key_len = strlen(key);
    const char *p = body;

    while ((p = strstr(p, key)) != NULL) {
        if (p != body && *(p - 1) != '&') {
            ++p;
            continue;
        }

        if (p[key_len] != '=') {
            ++p;
            continue;
        }

        const char *value_start = p + key_len + 1;
        const char *value_end = strchr(value_start, '&');
        size_t raw_len = value_end ? (size_t)(value_end - value_start) : strlen(value_start);

        if (raw_len >= value_size) {
            raw_len = value_size - 1;
        }

        char raw[128] = {0};
        memcpy(raw, value_start, raw_len);
        url_decode(value, value_size, raw);
        return true;
    }

    return false;
}

static esp_err_t read_http_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (req->content_len <= 0 || req->content_len >= (int)buf_size) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_send(req, "payload too large", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    int total = 0;
    while (total < req->content_len) {
        int ret = httpd_req_recv(req, buf + total, req->content_len - total);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        total += ret;
    }

    buf[total] = '\0';
    return ESP_OK;
}

static esp_err_t wifi_portal_connect_sta(const char *ssid, const char *password)
{
    wifi_config_t sta_cfg = {0};
    copy_text((char *)sta_cfg.sta.ssid, sizeof(sta_cfg.sta.ssid), ssid);
    copy_text((char *)sta_cfg.sta.password, sizeof(sta_cfg.sta.password), password);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    if (err != ESP_OK) {
        return err;
    }

    return esp_wifi_connect();
}

static int rssi_to_bars(int rssi)
{
    if (rssi >= -50) {
        return 4;
    }
    if (rssi >= -60) {
        return 3;
    }
    if (rssi >= -70) {
        return 2;
    }
    if (rssi >= -80) {
        return 1;
    }
    return 0;
}

static bool is_encrypted(wifi_auth_mode_t auth_mode)
{
    return auth_mode != WIFI_AUTH_OPEN;
}

static bool append_json(char *buf, size_t cap, int *pos, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *pos, cap - (size_t)(*pos), fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= cap - (size_t)(*pos)) {
        return false;
    }

    *pos += n;
    return true;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, index_end - index_start);
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    wifi_portal_state_t state = {0};
    wifi_portal_get_state(&state);

    char json[256];
    int len = snprintf(json,
                       sizeof(json),
                       "{\"status\":\"%s\",\"ip\":\"%s\",\"reason\":\"%s\",\"ap_ssid\":\"%s\"}",
                       wifi_portal_status_string(state.status),
                       state.sta_ip,
                       state.fail_reason,
                       state.ap_ssid);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, json, len);
    return ESP_OK;
}

static esp_err_t connect_post_handler(httpd_req_t *req)
{
    char body[WIFI_PORTAL_MAX_FORM_BODY] = {0};
    if (read_http_body(req, body, sizeof(body)) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "invalid body", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    char ssid[64] = {0};
    char password[64] = {0};
    bool has_ssid = get_form_value(body, "ssid", ssid, sizeof(ssid));
    (void)get_form_value(body, "password", password, sizeof(password));

    if (!has_ssid || ssid[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "ssid required", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "try connect ssid=%s", ssid);
    portal_set_state(WIFI_PORTAL_STATUS_CONNECTING, "", "");
    s_ctx.boot_connect_pending = false;
    s_ctx.boot_connect_retry = 0;

    esp_err_t err = wifi_portal_connect_sta(ssid, password);
    if (err != ESP_OK) {
        char reason[64] = {0};
        snprintf(reason, sizeof(reason), "start connect failed (%s)", esp_err_to_name(err));
        portal_set_state(WIFI_PORTAL_STATUS_FAILED, "", reason);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "connect failed", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = {0};
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.max = 120;

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"scan_failed\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "[]", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (ap_count > WIFI_PORTAL_MAX_SCAN_AP) {
        ap_count = WIFI_PORTAL_MAX_SCAN_AP;
    }

    wifi_ap_record_t ap_records[WIFI_PORTAL_MAX_SCAN_AP];
    uint16_t actual_count = ap_count;
    esp_wifi_scan_get_ap_records(&actual_count, ap_records);

    char *json = (char *)calloc(1, WIFI_PORTAL_JSON_BUF_SIZE);
    if (json == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"no_memory\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    int pos = 0;
    json[pos++] = '[';

    for (int i = 0; i < actual_count; ++i) {
        if (i > 0) {
            json[pos++] = ',';
        }

        char escaped_ssid[128] = {0};
        int epos = 0;
        const char *src = (const char *)ap_records[i].ssid;
        for (int j = 0; src[j] != '\0' && epos < (int)sizeof(escaped_ssid) - 2; ++j) {
            if (src[j] == '"' || src[j] == '\\') {
                escaped_ssid[epos++] = '\\';
            }
            if ((unsigned char)src[j] >= 0x20U) {
                escaped_ssid[epos++] = src[j];
            }
        }
        escaped_ssid[epos] = '\0';

        if (!append_json(json,
                         WIFI_PORTAL_JSON_BUF_SIZE,
                         &pos,
                         "{\"ssid\":\"%s\",\"rssi\":%d,\"bars\":%d,\"encrypted\":%s,\"channel\":%d}",
                         escaped_ssid,
                         ap_records[i].rssi,
                         rssi_to_bars(ap_records[i].rssi),
                         is_encrypted(ap_records[i].authmode) ? "true" : "false",
                         ap_records[i].primary)) {
            free(json);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"error\":\"json_overflow\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }

    if ((size_t)(pos + 2) >= WIFI_PORTAL_JSON_BUF_SIZE) {
        free(json);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"json_overflow\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    json[pos++] = ']';
    json[pos] = '\0';

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, json, pos);

    free(json);
    return ESP_OK;
}

/* Captive-portal auto-popup: redirect every unmatched probe URL (Android
 * /generate_204, Apple /hotspot-detect.html, Windows /connecttest.txt, etc.)
 * to the portal root with an absolute URL, so the OS shows the sign-in page. */
static esp_err_t http_redirect_to_portal(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, "redirect", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    return http_redirect_to_portal(req);
}

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
};

static const httpd_uri_t connect_uri = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = connect_post_handler,
};

static const httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_get_handler,
};

static const httpd_uri_t scan_uri = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_get_handler,
};

static esp_err_t start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Reduce max open sockets to fit LWIP limits on some sdkconfig (avoid runtime error)
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_ctx.http_server, &config);
    if (err != ESP_OK) {
        return err;
    }

    httpd_register_uri_handler(s_ctx.http_server, &root_uri);
    httpd_register_uri_handler(s_ctx.http_server, &connect_uri);
    httpd_register_uri_handler(s_ctx.http_server, &status_uri);
    httpd_register_uri_handler(s_ctx.http_server, &scan_uri);
    httpd_register_err_handler(s_ctx.http_server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    httpd_register_err_handler(s_ctx.http_server, HTTPD_405_METHOD_NOT_ALLOWED, http_404_error_handler);

    return ESP_OK;
}

static void stop_web_server(void)
{
    if (s_ctx.http_server != NULL) {
        httpd_stop(s_ctx.http_server);
        s_ctx.http_server = NULL;
    }
}

static esp_err_t start_portal_services(void)
{
    if (s_ctx.portal_running) {
        return ESP_OK;
    }

    wifi_config_t ap_cfg = {0};
    char ap_ssid[33] = {0};
    build_ap_config(&ap_cfg, ap_ssid, sizeof(ap_ssid));

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = start_web_server();
    if (err != ESP_OK) {
        return err;
    }

    dns_server_config_t dns_cfg = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    s_ctx.dns_server = dns_server_start(&dns_cfg);
    if (s_ctx.dns_server == NULL) {
        stop_web_server();
        return ESP_FAIL;
    }

    if (s_ctx.lock != NULL) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        copy_text(s_ctx.state.ap_ssid, sizeof(s_ctx.state.ap_ssid), ap_ssid);
        xSemaphoreGive(s_ctx.lock);
    }

    s_ctx.portal_running = true;
    ESP_LOGI(TAG, "wifi portal started, ap ssid=%s", ap_ssid);
    return ESP_OK;
}

static void stop_portal_services(void)
{
    if (s_ctx.dns_server != NULL) {
        dns_server_stop(s_ctx.dns_server);
        s_ctx.dns_server = NULL;
    }

    stop_web_server();

    if (s_ctx.lock != NULL) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.state.ap_ssid[0] = '\0';
        xSemaphoreGive(s_ctx.lock);
    }

    s_ctx.portal_running = false;
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = (const wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG,
                 "client " MACSTR " joined, aid=%d",
                 MAC2STR(event->mac),
                 event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = (const wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG,
                 "client " MACSTR " left, aid=%d, reason=%d",
                 MAC2STR(event->mac),
                 event->aid,
                 event->reason);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = (const wifi_event_sta_disconnected_t *)event_data;
        if (s_ctx.boot_connect_pending) {
            if (s_ctx.boot_connect_retry < WIFI_PORTAL_BOOT_RETRY_MAX) {
                s_ctx.boot_connect_retry++;
                ESP_LOGW(TAG,
                         "saved wifi connect failed (%u), retry %u/%u",
                         (unsigned int)event->reason,
                         (unsigned int)s_ctx.boot_connect_retry,
                         (unsigned int)WIFI_PORTAL_BOOT_RETRY_MAX);
                (void)esp_wifi_connect();
                return;
            }

            s_ctx.boot_connect_pending = false;
            s_ctx.boot_connect_retry = 0;

            char reason[64] = {0};
            snprintf(reason,
                     sizeof(reason),
                     "%s (%u)",
                     disconnect_reason_text(event->reason),
                     (unsigned int)event->reason);

            portal_set_state(WIFI_PORTAL_STATUS_FAILED, "", reason);

            esp_err_t err = start_portal_services();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "fallback portal start failed: %s", esp_err_to_name(err));
            } else {
                portal_set_state(WIFI_PORTAL_STATUS_IDLE, "", "");
                ESP_LOGW(TAG, "saved wifi unavailable, fallback to portal");
            }

            return;
        }

        if (s_ctx.portal_running) {
            wifi_portal_state_t snapshot = {0};
            wifi_portal_get_state(&snapshot);

            if (snapshot.status == WIFI_PORTAL_STATUS_CONNECTING ||
                snapshot.status == WIFI_PORTAL_STATUS_CONNECTED) {
                char reason[64] = {0};
                snprintf(reason,
                         sizeof(reason),
                         "%s (%u)",
                         disconnect_reason_text(event->reason),
                         (unsigned int)event->reason);
                portal_set_state(WIFI_PORTAL_STATUS_FAILED, "", reason);
            }

            return;
        }

        portal_set_state(WIFI_PORTAL_STATUS_CONNECTING, "", "");
        ESP_LOGW(TAG,
                 "sta disconnected, reconnecting: %s (%u)",
                 disconnect_reason_text(event->reason),
                 (unsigned int)event->reason);
        (void)esp_wifi_connect();
    }
}

static void ip_event_handler(void *arg,
                             esp_event_base_t event_base,
                             int32_t event_id,
                             void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id != IP_EVENT_STA_GOT_IP) {
        return;
    }

    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
    char sta_ip[16] = {0};
    inet_ntoa_r(event->ip_info.ip.addr, sta_ip, sizeof(sta_ip));

    s_ctx.boot_connect_pending = false;
    s_ctx.boot_connect_retry = 0;
    portal_set_state(WIFI_PORTAL_STATUS_CONNECTED, sta_ip, "");

    wifi_config_t sta_cfg = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &sta_cfg) == ESP_OK && sta_cfg.sta.ssid[0] != '\0') {
        esp_err_t save_err = nvs_save_sta_credentials((const char *)sta_cfg.sta.ssid,
                                                      (const char *)sta_cfg.sta.password);
        if (save_err != ESP_OK) {
            ESP_LOGW(TAG, "save wifi credentials failed: %s", esp_err_to_name(save_err));
        }
    }

    if (s_ctx.portal_running) {
        stop_portal_services();
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "switch to STA mode failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "portal closed, switched to STA mode");
        }
    }

    ESP_LOGI(TAG, "sta connected, ip=%s", sta_ip);
}

static esp_err_t nvs_init_with_recovery(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t wifi_portal_start(const wifi_portal_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_ctx.started) {
        return ESP_OK;
    }

    if (s_ctx.lock == NULL) {
        s_ctx.lock = xSemaphoreCreateMutex();
        if (s_ctx.lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    portal_set_state(WIFI_PORTAL_STATUS_IDLE, "", "");

    esp_err_t err = nvs_init_with_recovery();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_cfg);
    if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE) {
        goto fail;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        goto fail;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              wifi_event_handler,
                                              NULL,
                                              &s_ctx.wifi_handler);
    if (err != ESP_OK) {
        goto fail;
    }

    err = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              ip_event_handler,
                                              NULL,
                                              &s_ctx.ip_handler);
    if (err != ESP_OK) {
        goto fail;
    }

    cache_runtime_config(config);

    char saved_ssid[33] = {0};
    char saved_password[65] = {0};
    bool has_saved_credentials = false;
    err = nvs_load_sta_credentials(saved_ssid,
                                   sizeof(saved_ssid),
                                   saved_password,
                                   sizeof(saved_password),
                                   &has_saved_credentials);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "load wifi credentials failed: %s", esp_err_to_name(err));
        goto fail;
    }

    if (has_saved_credentials) {
        wifi_config_t sta_cfg = {0};
        copy_text((char *)sta_cfg.sta.ssid, sizeof(sta_cfg.sta.ssid), saved_ssid);
        copy_text((char *)sta_cfg.sta.password, sizeof(sta_cfg.sta.password), saved_password);
        sta_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        sta_cfg.sta.pmf_cfg.capable = true;
        sta_cfg.sta.pmf_cfg.required = false;

        err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) {
            goto fail;
        }

        err = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
        if (err != ESP_OK) {
            goto fail;
        }

        err = esp_wifi_start();
        if (err != ESP_OK) {
            goto fail;
        }

        (void)esp_wifi_set_ps(WIFI_PS_NONE);

        s_ctx.boot_connect_pending = true;
        s_ctx.boot_connect_retry = 0;
        portal_set_state(WIFI_PORTAL_STATUS_CONNECTING, "", "");

        err = esp_wifi_connect();
        if (err != ESP_OK) {
            goto fail;
        }

        s_ctx.portal_running = false;
        s_ctx.started = true;
        ESP_LOGI(TAG, "found saved wifi, connect directly: %s", saved_ssid);
        return ESP_OK;
    }

    s_ctx.boot_connect_pending = false;
    s_ctx.boot_connect_retry = 0;

    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        goto fail;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        goto fail;
    }

    (void)esp_wifi_set_ps(WIFI_PS_NONE);

    err = start_portal_services();
    if (err != ESP_OK) {
        goto fail;
    }

    s_ctx.started = true;
    ESP_LOGI(TAG, "no saved wifi, portal mode enabled");
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "wifi portal start failed: %s", esp_err_to_name(err));
    wifi_portal_stop();
    return err;
}

esp_err_t wifi_portal_stop(void)
{
    stop_portal_services();

    if (s_ctx.ip_handler != NULL) {
        (void)esp_event_handler_instance_unregister(IP_EVENT,
                                                    IP_EVENT_STA_GOT_IP,
                                                    s_ctx.ip_handler);
        s_ctx.ip_handler = NULL;
    }

    if (s_ctx.wifi_handler != NULL) {
        (void)esp_event_handler_instance_unregister(WIFI_EVENT,
                                                    ESP_EVENT_ANY_ID,
                                                    s_ctx.wifi_handler);
        s_ctx.wifi_handler = NULL;
    }

    (void)esp_wifi_stop();
    (void)esp_wifi_deinit();

    s_ctx.boot_connect_pending = false;
    s_ctx.boot_connect_retry = 0;
    portal_set_state(WIFI_PORTAL_STATUS_IDLE, "", "");

    s_ctx.started = false;
    return ESP_OK;
}