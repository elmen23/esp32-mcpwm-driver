/**
 * @file    wifi_ap.hpp
 * @brief   WiFi manager — AP mode (primary), optional STA mode.
 *
 * Manages NVS init, netif, event loop, and WiFi lifecycle.
 * Default AP: SSID "ESP32-MCPWM" / password "mcpwm2024".
 */

#pragma once

#include <cstdint>
#include "esp_err.h"

namespace wifi {

/* ─── Default AP Configuration ─── */
inline constexpr const char *AP_SSID         = "ESP32-MCPWM";
inline constexpr const char *AP_PASS         = "mcpwm2024";
inline constexpr uint8_t     AP_CHANNEL      = 1;
inline constexpr uint8_t     AP_MAX_CONN     = 4;

/* ─── Operation Mode ─── */
enum class Mode : uint8_t {
    NONE = 0,
    AP   = 1,
    STA  = 2,
};

/* ════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════ */

/**
 * Initialise NVS, netif, event loop, and start WiFi in AP mode.
 * Call once during boot.  Safe to call if NVS is corrupted (auto-repair).
 */
esp_err_t init_ap() noexcept;

/**
 * Initialise and connect as WiFi station (optional).
 * @param ssid  Router SSID (null-terminated).
 * @param pass  Router password (null-terminated, may be "" for open).
 */
esp_err_t init_sta(const char *ssid, const char *pass) noexcept;

/**
 * True if currently connected to a router (STA mode only).
 */
bool is_connected() noexcept;

/**
 * Current operation mode.
 */
Mode get_mode() noexcept;

/**
 * Human-readable mode string: "AP", "STA", or "NONE".
 */
const char *get_mode_str() noexcept;

/**
 * Local IP address as a C-string (e.g. "192.168.4.1").
 */
const char *get_ip_str() noexcept;

/**
 * Gracefully stop WiFi and release resources.
 */
void stop() noexcept;

}  // namespace wifi
