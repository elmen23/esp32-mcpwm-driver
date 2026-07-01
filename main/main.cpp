/**
 * @file    main.cpp
 * @brief   ESP32 MCPWM Driver — Entry Point
 *
 * Industrial PWM controller firmware built on ESP-IDF.
 * Phase 1: Minimal boot — verifies build system & CI/CD.
 */

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr const char *TAG = "APP";

// ────────────────────────────────────────────────────────────
//  app_main  —  ESP-IDF entry point
// ────────────────────────────────────────────────────────────

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   ESP32 MCPWM Driver  —  Phase 1    ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");

    /*── Chip info ──*/
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "Chip: %s | Cores: %u | Rev: %u",
             (chip_info.model == CHIP_ESP32) ? "ESP32" : "Unknown",
             chip_info.cores,
             chip_info.revision);

    ESP_LOGI(TAG, "Features: %s%s%s",
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
             (chip_info.features & CHIP_FEATURE_BT)        ? "BT "  : "",
             (chip_info.features & CHIP_FEATURE_BLE)       ? "BLE"  : "");

    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());

    /*── Main loop — idle, waiting for components ──*/
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGD(TAG, "Heartbeat — heap: %" PRIu32,
                 esp_get_free_heap_size());
    }
}
