/**
 * @file    nvs_config.cpp
 * @brief   NVS-based configuration persistence implementation.
 *
 * Uses the "mcpwm" NVS namespace.
 * NVS must be initialised before calling these functions
 * (handled by wifi::init_ap normally, but also works standalone).
 */

#include "nvs_config.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>

static constexpr const char *TAG      = "NVS";
static constexpr const char *NS_NAME  = "mcpwm";

/* ── NVS keys ── */
static constexpr const char *KEY_ENABLE   = "enable";
static constexpr const char *KEY_FREQ     = "freq_hz";
static constexpr const char *KEY_DUTY     = "duty_pct";
static constexpr const char *KEY_DT_RED   = "dt_red";
static constexpr const char *KEY_DT_FED   = "dt_fed";

namespace storage {

esp_err_t save_config(const mcpwm::Config &cfg) noexcept {
    nvs_handle_t handle {};
    esp_err_t err = nvs_open(NS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_u8(handle,  KEY_ENABLE, cfg.enable ? 1 : 0);
    nvs_set_u32(handle, KEY_FREQ,   cfg.frequency_hz);
    nvs_set_u32(handle, KEY_DUTY,   static_cast<uint32_t>(cfg.duty_percent * 10.0f));
    nvs_set_u32(handle, KEY_DT_RED, cfg.dead_time_red_ns);
    nvs_set_u32(handle, KEY_DT_FED, cfg.dead_time_fed_ns);

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Config saved — %" PRIu32 " kHz  %.1f %%  RED:%" PRIu32 "  FED:%" PRIu32,
                 cfg.frequency_hz / 1000, static_cast<double>(cfg.duty_percent),
                 cfg.dead_time_red_ns, cfg.dead_time_fed_ns);
    }

    nvs_close(handle);
    return err;
}

esp_err_t load_config(mcpwm::Config &cfg) noexcept {
    nvs_handle_t handle {};
    esp_err_t err = nvs_open(NS_NAME, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No saved config — using defaults: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t  u8  = 0;
    uint32_t u32 = 0;

    if (nvs_get_u8(handle, KEY_ENABLE, &u8) == ESP_OK)
        cfg.enable = (u8 != 0);

    if (nvs_get_u32(handle, KEY_FREQ, &u32) == ESP_OK)
        cfg.frequency_hz = u32;

    if (nvs_get_u32(handle, KEY_DUTY, &u32) == ESP_OK)
        cfg.duty_percent = static_cast<float>(u32) / 10.0f;

    if (nvs_get_u32(handle, KEY_DT_RED, &u32) == ESP_OK)
        cfg.dead_time_red_ns = u32;

    if (nvs_get_u32(handle, KEY_DT_FED, &u32) == ESP_OK)
        cfg.dead_time_fed_ns = u32;

    nvs_close(handle);

    ESP_LOGI(TAG, "Config loaded — %" PRIu32 " kHz  %.1f %%  RED:%" PRIu32 "  FED:%" PRIu32,
             cfg.frequency_hz / 1000, static_cast<double>(cfg.duty_percent),
             cfg.dead_time_red_ns, cfg.dead_time_fed_ns);
    return ESP_OK;
}

esp_err_t erase_config() noexcept {
    nvs_handle_t handle {};
    esp_err_t err = nvs_open(NS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erase: nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_erase_all(handle);
    nvs_close(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config erased (factory reset)");
    } else {
        ESP_LOGE(TAG, "Erase failed: %s", esp_err_to_name(err));
    }
    return err;
}

}  // namespace storage
