/**
 * @file    wifi_ap.cpp
 * @brief   WiFi manager implementation — AP mode with NVS init and event handling.
 *
 * Lifecycle:
 *   1. NVS flash init (auto-repair on corruption)
 *   2. TCP/IP netif init
 *   3. Default event loop creation
 *   4. WiFi init + AP config + start
 *
 * Station mode (init_sta) is also supported for future use.
 */

#include "wifi_ap.hpp"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <cstring>

static constexpr const char *TAG = "WiFi";

/* ════════════════════════════════════════════
 *  File‑local state
 * ════════════════════════════════════════════ */

static esp_netif_t *s_netif           = nullptr;
static wifi::Mode  s_mode             = wifi::Mode::NONE;
static char        s_ip_str[16]       = "0.0.0.0";
static bool        s_sta_connected    = false;
static bool        s_running          = false;   // guard against reconnect after stop()

/* ════════════════════════════════════════════
 *  Event handler
 * ════════════════════════════════════════════ */

static void event_handler(void * /*arg*/,
                          esp_event_base_t base,
                          int32_t id,
                          void *data) noexcept
{
    /* ── AP events ── */
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        const auto *ev = static_cast<wifi_event_ap_staconnected_t *>(data);
        ESP_LOGI(TAG, "Station connected — MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 ev->mac[0], ev->mac[1], ev->mac[2],
                 ev->mac[3], ev->mac[4], ev->mac[5]);
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "Station disconnected");
    }

    /* ── STA events ── */
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        s_sta_connected = true;
        ESP_LOGI(TAG, "Connected to router");
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        if (s_running) {
            ESP_LOGW(TAG, "Disconnected — reconnecting ...");
            esp_wifi_connect();
        }
    }

    /* ── IP acquisition ── */
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto *ev = static_cast<ip_event_got_ip_t *>(data);
        esp_ip4addr_ntoa(&ev->ip_info.ip, s_ip_str, sizeof(s_ip_str));
        ESP_LOGI(TAG, "Got IP: %s", s_ip_str);
    }
}

/* ════════════════════════════════════════════
 *  Common init  (NVS + netif + event loop)
 * ════════════════════════════════════════════ */

static esp_err_t common_init() noexcept {
    /* ── NVS flash — auto-repair on corruption ── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt — erasing and re-initialising");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_flash_init");

    /* ── TCP/IP stack ── */
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif_init");

    /* ── Default event loop ── */
    ESP_RETURN_ON_ERROR(
        esp_event_loop_create_default(), TAG, "event_loop");

    return ESP_OK;
}

/* ════════════════════════════════════════════
 *  Public API Implementation
 * ════════════════════════════════════════════ */

namespace wifi {

esp_err_t init_ap() noexcept {
    ESP_RETURN_ON_ERROR(common_init(), TAG, "common_init");

    s_netif = esp_netif_create_default_wifi_ap();

    /* ── WiFi init ── */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi_init");

    /* ── Register event handler ── */
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID,
            &event_handler, nullptr, nullptr),
        TAG, "event_handler");

    /* ── AP configuration ── */
    wifi_config_t ap_cfg {};
    std::strncpy(reinterpret_cast<char *>(ap_cfg.ap.ssid),
                 AP_SSID, sizeof(ap_cfg.ap.ssid) - 1);
    std::strncpy(reinterpret_cast<char *>(ap_cfg.ap.password),
                 AP_PASS, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.ssid_len       = 0;            // null-terminated
    ap_cfg.ap.channel        = AP_CHANNEL;
    ap_cfg.ap.authmode       = WIFI_AUTH_WPA_WPA2_PSK;
    ap_cfg.ap.max_connection = AP_MAX_CONN;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP),     TAG, "set_mode");
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_config(WIFI_IF_AP, &ap_cfg),             TAG, "set_config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(),                     TAG, "wifi_start");

    s_mode = Mode::AP;
    s_running = true;
    std::strcpy(s_ip_str, "192.168.4.1");

    ESP_LOGI(TAG, "AP started — SSID: '%s'  IP: %s  CH: %u",
             AP_SSID, s_ip_str, AP_CHANNEL);
    return ESP_OK;
}

esp_err_t init_sta(const char *ssid, const char *pass) noexcept {
    ESP_RETURN_ON_ERROR(common_init(), TAG, "common_init");

    s_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi_init");

    /* ── Register handlers for STA + IP events ── */
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID,
            &event_handler, nullptr, nullptr),
        TAG, "event_handler");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP,
            &event_handler, nullptr, nullptr),
        TAG, "ip_handler");

    /* ── STA configuration ── */
    wifi_config_t sta_cfg {};
    std::strncpy(reinterpret_cast<char *>(sta_cfg.sta.ssid),
                 ssid, sizeof(sta_cfg.sta.ssid) - 1);
    if (pass && std::strlen(pass) > 0) {
        std::strncpy(reinterpret_cast<char *>(sta_cfg.sta.password),
                     pass, sizeof(sta_cfg.sta.password) - 1);
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA),    TAG, "set_mode");
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg),            TAG, "set_config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(),                      TAG, "wifi_start");

    s_mode = Mode::STA;
    s_running = true;
    ESP_LOGI(TAG, "STA connecting to '%s' ...", ssid);
    return ESP_OK;
}

bool is_connected() noexcept {
    return s_sta_connected;
}

Mode get_mode() noexcept {
    return s_mode;
}

const char *get_mode_str() noexcept {
    switch (s_mode) {
        case Mode::AP:  return "AP";
        case Mode::STA: return "STA";
        default:        return "NONE";
    }
}

const char *get_ip_str() noexcept {
    return s_ip_str;
}

void stop() noexcept {
    s_running = false;
    esp_wifi_stop();
    esp_wifi_deinit();
    s_mode = Mode::NONE;
    ESP_LOGI(TAG, "WiFi stopped");
}

}  // namespace wifi
