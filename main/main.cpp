/**
 * @file    main.cpp
 * @brief   ESP32 MCPWM Driver — System Entry Point.
 *
 * Init sequence:
 *   1. NVS load config (or defaults)
 *   2. MCPWM driver init + apply saved config
 *   3. WiFi AP mode start
 *   4. HTTP server start (REST API + web UI)
 *   5. Feedback monitor task (core 1)
 *
 * All outputs start DISABLED — user must enable via web UI.
 */

#include "mcpwm_driver.hpp"
#include "wifi_ap.hpp"
#include "http_server.hpp"
#include "nvs_config.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr const char *TAG = "APP";

/* ── Forward declarations ── */
static void feedback_task(void * /*pv*/) noexcept;

/* ════════════════════════════════════════════
 *  app_main  —  ESP-IDF entry point
 * ════════════════════════════════════════════ */

extern "C" void app_main() {
    ESP_LOGI(TAG, "╔══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║     ESP32 MCPWM Driver  —  v1.0.0       ║");
    ESP_LOGI(TAG, "║  Industrial Half-Bridge PWM Controller  ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════╝");

    /* ── 0. Chip info ── */
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "SoC: ESP32 r%u | %u core(s) | %" PRIu32 " kB free heap",
             chip.revision, chip.cores,
             esp_get_free_heap_size() / 1024);

    /* ── 1. Load saved config (or use defaults) ── */
    mcpwm::Config cfg {};
    if (storage::load_config(cfg) != ESP_OK) {
        ESP_LOGI(TAG, "No saved config — using factory defaults");
    }

    /* ── 2. Init MCPWM ── */
    ESP_LOGI(TAG, "[1/4] Initialising MCPWM ...");
    esp_err_t err = mcpwm::init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MCPWM init FAILED — reboot in 5 s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    /* Apply saved config (outputs stay OFF) */
    mcpwm::apply_config(cfg);

    /* ── 3. Start WiFi AP ── */
    ESP_LOGI(TAG, "[2/4] Starting WiFi AP ...");
    err = wifi::init_ap();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WiFi init FAILED — continuing without network");
    }

    /* ── 4. Start HTTP server ── */
    ESP_LOGI(TAG, "[3/4] Starting HTTP server ...");
    err = web::init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP server FAILED — continuing with MCPWM only");
    }

    /* ── 5. Start feedback monitor task ── */
    ESP_LOGI(TAG, "[4/4] Starting feedback monitor ...");
    xTaskCreatePinnedToCore(
        feedback_task, "feedback", 4096, nullptr,
        tskIDLE_PRIORITY + 2, nullptr, 1);

    /* ── Ready ── */
    ESP_LOGI(TAG, "════════════════════════════════════════════");
    ESP_LOGI(TAG, " System ready!");
    ESP_LOGI(TAG, "  WiFi AP: %s  |  IP: %s",
             wifi::AP_SSID, wifi::get_ip_str());
    ESP_LOGI(TAG, "  Web UI:  http://%s", wifi::get_ip_str());
    ESP_LOGI(TAG, "  PWM:     GPIO %d / %d",
             18, 19);
    ESP_LOGI(TAG, "════════════════════════════════════════════");

    /* ── Main loop — periodic logging + auto-save ── */
    uint32_t tick = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        tick += 5;

        mcpwm::Config cur;
        mcpwm::get_config(cur);

        ESP_LOGI(TAG, "[%3" PRIu32 "s]  %s  |  %" PRIu32 " kHz  %.1f %%  "
                       "RED:%" PRIu32 "  FED:%" PRIu32,
                 tick,
                 cur.enable ? "RUN" : "STOP",
                 cur.frequency_hz / 1000,
                 static_cast<double>(cur.duty_percent),
                 cur.dead_time_red_ns,
                 cur.dead_time_fed_ns);

        /* Auto-save every 30 s */
        if (tick % 30 == 0) {
            storage::save_config(cur);
        }
    }
}

/* ════════════════════════════════════════════
 *  Feedback task  (core 1, 250 ms interval)
 * ════════════════════════════════════════════ */

static void feedback_task(void * /*pv*/) noexcept {
    TickType_t last = xTaskGetTickCount();
    constexpr TickType_t interval = pdMS_TO_TICKS(250);

    while (true) {
        mcpwm::Feedback fb;
        mcpwm::get_feedback(fb);
        // In production: read ADC / sensors here
        vTaskDelayUntil(&last, interval);
    }
}
