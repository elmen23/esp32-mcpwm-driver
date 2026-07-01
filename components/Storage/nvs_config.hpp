/**
 * @file    nvs_config.hpp
 * @brief   NVS-based configuration persistence.
 *
 * Saves/loads MCPWM config to non-volatile storage so settings
 * survive power cycles.  Returns sensible defaults on first boot
 * or if NVS is corrupted.
 */

#pragma once

#include "mcpwm_driver.hpp"
#include "esp_err.h"

namespace storage {

/**
 * Save the current MCPWM configuration to NVS.
 * @return ESP_OK on success, or ESP_FAIL if NVS is not initialised.
 */
esp_err_t save_config(const mcpwm::Config &cfg) noexcept;

/**
 * Load configuration from NVS.  If no saved config exists,
 * returns the provided defaults unchanged.
 * @param cfg  [in/out] — populated with saved values on success.
 */
esp_err_t load_config(mcpwm::Config &cfg) noexcept;

/**
 * Erase all stored configuration (factory reset).
 */
esp_err_t erase_config() noexcept;

}  // namespace storage
