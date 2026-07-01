/**
 * @file    http_server.cpp
 * @brief   HTTP server implementation — REST API + embedded web assets.
 *
 * Routes:
 *   GET  /              → index.html          (text/html)
 *   GET  /style.css     → stylesheet           (text/css)
 *   GET  /script.js     → client JS            (application/javascript)
 *   GET  /api/config    → read MCPWM config    (JSON)
 *   POST /api/config    → update MCPWM config  (JSON)
 *   GET  /api/status    → status + feedback    (JSON)
 *   POST /api/estop     → emergency stop       (JSON)
 */

#include "http_server.hpp"
#include "mcpwm_driver.hpp"
#include "wifi_ap.hpp"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <cstring>

static constexpr const char *TAG = "HTTP";

/* ════════════════════════════════════════════
 *  Embedded web assets  (EMBED_FILES via CMake)
 * ════════════════════════════════════════════ */

extern "C" {
    extern const uint8_t index_html_start[]  asm("_binary_index_html_start");
    extern const uint8_t index_html_end[]    asm("_binary_index_html_end");
    extern const uint8_t style_css_start[]   asm("_binary_style_css_start");
    extern const uint8_t style_css_end[]     asm("_binary_style_css_end");
    extern const uint8_t script_js_start[]   asm("_binary_script_js_start");
    extern const uint8_t script_js_end[]     asm("_binary_script_js_end");
}

/* ── Static file mapping ── */
struct Asset {
    const char   *uri;
    const char   *mime;
    const uint8_t *data;
    size_t         len;
};

static const Asset s_assets[] = {
    { "/",          "text/html; charset=utf-8",       index_html_start,
      static_cast<size_t>(index_html_end - index_html_start) },
    { "/index.html","text/html; charset=utf-8",       index_html_start,
      static_cast<size_t>(index_html_end - index_html_start) },
    { "/style.css", "text/css; charset=utf-8",        style_css_start,
      static_cast<size_t>(style_css_end - style_css_start) },
    { "/script.js", "application/javascript",          script_js_start,
      static_cast<size_t>(script_js_end - script_js_start) },
};

/* ════════════════════════════════════════════
 *  File‑local server handle
 * ════════════════════════════════════════════ */

static httpd_handle_t s_server = nullptr;

/* ════════════════════════════════════════════
 *  Helpers
 * ════════════════════════════════════════════ */

static void add_cors_headers(httpd_req_t *req) noexcept {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin",  "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static void send_json(httpd_req_t *req, cJSON *root) noexcept {
    char *str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    add_cors_headers(req);
    httpd_resp_sendstr(req, str);
    cJSON_free(str);
}

static void send_json_err(httpd_req_t *req, int code, const char *msg) noexcept {
    httpd_resp_set_type(req, "application/json");
    add_cors_headers(req);
    httpd_resp_set_status(req, std::to_string(code).c_str());
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "error", msg);
    char *str = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, str);
    cJSON_free(str);
    cJSON_Delete(root);
}

/* ════════════════════════════════════════════
 *  Static file handler
 * ════════════════════════════════════════════ */

static esp_err_t handler_static(httpd_req_t *req) noexcept {
    for (const auto &a : s_assets) {
        if (std::strcmp(req->uri, a.uri) == 0) {
            httpd_resp_set_type(req, a.mime);
            httpd_resp_set_hdr(req, "Cache-Control",
                               "no-cache, no-store, must-revalidate");
            httpd_resp_send(req,
                reinterpret_cast<const char *>(a.data), a.len);
            return ESP_OK;
        }
    }
    // Fallback → index.html
    httpd_resp_set_type(req, s_assets[0].mime);
    httpd_resp_send(req,
        reinterpret_cast<const char *>(s_assets[0].data), s_assets[0].len);
    return ESP_OK;
}

/* ════════════════════════════════════════════
 *  GET /api/config
 * ════════════════════════════════════════════ */

static esp_err_t handler_get_config(httpd_req_t *req) noexcept {
    mcpwm::Config cfg;
    mcpwm::get_config(cfg);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root,   "enable",         cfg.enable);
    cJSON_AddNumberToObject(root, "frequency",      cfg.frequency_hz / 1000.0);
    cJSON_AddNumberToObject(root, "duty",           cfg.duty_percent);
    cJSON_AddNumberToObject(root, "dead_time_red",  static_cast<double>(cfg.dead_time_red_ns));
    cJSON_AddNumberToObject(root, "dead_time_fed",  static_cast<double>(cfg.dead_time_fed_ns));

    send_json(req, root);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ════════════════════════════════════════════
 *  POST /api/config
 * ════════════════════════════════════════════ */

static esp_err_t handler_post_config(httpd_req_t *req) noexcept {
    char buf[256] {};
    const int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        send_json_err(req, 400, "Empty body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        send_json_err(req, 400, "Invalid JSON");
        return ESP_FAIL;
    }

    mcpwm::Config cfg;
    mcpwm::get_config(cfg);

    const cJSON *item;
    item = cJSON_GetObjectItem(root, "enable");
    if (cJSON_IsBool(item)) cfg.enable = (item->valueint != 0);

    item = cJSON_GetObjectItem(root, "frequency");
    if (cJSON_IsNumber(item)) {
        cfg.frequency_hz = static_cast<uint32_t>(item->valuedouble * 1000.0);
    }

    item = cJSON_GetObjectItem(root, "duty");
    if (cJSON_IsNumber(item)) {
        cfg.duty_percent = static_cast<float>(item->valuedouble);
    }

    item = cJSON_GetObjectItem(root, "dead_time_red");
    if (cJSON_IsNumber(item)) {
        cfg.dead_time_red_ns = static_cast<uint32_t>(item->valuedouble);
    }

    item = cJSON_GetObjectItem(root, "dead_time_fed");
    if (cJSON_IsNumber(item)) {
        cfg.dead_time_fed_ns = static_cast<uint32_t>(item->valuedouble);
    }

    cJSON_Delete(root);

    esp_err_t err = mcpwm::apply_config(cfg);
    if (err != ESP_OK) {
        send_json_err(req, 500, "Failed to apply config");
        return ESP_FAIL;
    }

    return handler_get_config(req);  // echo back applied config
}

/* ════════════════════════════════════════════
 *  GET /api/status
 * ════════════════════════════════════════════ */

static esp_err_t handler_get_status(httpd_req_t *req) noexcept {
    mcpwm::Config cfg;
    mcpwm::get_config(cfg);

    mcpwm::Feedback fb;
    mcpwm::get_feedback(fb);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root,   "enable",      cfg.enable);
    cJSON_AddNumberToObject(root, "frequency",    cfg.frequency_hz / 1000.0);
    cJSON_AddNumberToObject(root, "duty",         cfg.duty_percent);
    cJSON_AddNumberToObject(root, "power",        fb.power_kw);
    cJSON_AddNumberToObject(root, "voltage",      fb.voltage);
    cJSON_AddNumberToObject(root, "current",      fb.current_a);
    cJSON_AddNumberToObject(root, "temperature",  fb.temperature_c);
    cJSON_AddBoolToObject(root,   "wifi_connected", wifi::is_connected());
    cJSON_AddStringToObject(root, "wifi_mode",     wifi::get_mode_str());
    cJSON_AddStringToObject(root, "wifi_ip",       wifi::get_ip_str());
    cJSON_AddNumberToObject(root, "uptime_sec",
        esp_timer_get_time() / 1'000'000.0);

    send_json(req, root);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ════════════════════════════════════════════
 *  POST /api/estop
 * ════════════════════════════════════════════ */

static esp_err_t handler_post_estop(httpd_req_t *req) noexcept {
    ESP_LOGW(TAG, "*** EMERGENCY STOP via HTTP ***");
    mcpwm::emergency_stop();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ESTOPPED");
    send_json(req, root);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ════════════════════════════════════════════
 *  OPTIONS  (CORS preflight)
 * ════════════════════════════════════════════ */

static esp_err_t handler_options(httpd_req_t *req) noexcept {
    add_cors_headers(req);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

/* ════════════════════════════════════════════
 *  Route table
 * ════════════════════════════════════════════ */

static const httpd_uri_t s_routes[] = {
    { "/",           HTTP_GET,     handler_static,      nullptr },
    { "/index.html", HTTP_GET,     handler_static,      nullptr },
    { "/style.css",  HTTP_GET,     handler_static,      nullptr },
    { "/script.js",  HTTP_GET,     handler_static,      nullptr },

    { "/api/config", HTTP_GET,     handler_get_config,  nullptr },
    { "/api/config", HTTP_POST,    handler_post_config, nullptr },
    { "/api/config", HTTP_OPTIONS, handler_options,     nullptr },

    { "/api/status", HTTP_GET,     handler_get_status,  nullptr },
    { "/api/status", HTTP_OPTIONS, handler_options,     nullptr },

    { "/api/estop",  HTTP_POST,    handler_post_estop,  nullptr },
    { "/api/estop",  HTTP_OPTIONS, handler_options,     nullptr },
};

/* ════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════ */

namespace web {

esp_err_t init() noexcept {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = static_cast<uint16_t>(sizeof(s_routes) / sizeof(s_routes[0]));
    cfg.stack_size       = 4096;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    for (const auto &route : s_routes) {
        httpd_register_uri_handler(s_server, &route);
    }

    ESP_LOGI(TAG, "HTTP server started on port %u", cfg.server_port);
    return ESP_OK;
}

void stop() noexcept {
    if (s_server) {
        httpd_stop(s_server);
        s_server = nullptr;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

}  // namespace web
