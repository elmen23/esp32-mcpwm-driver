/**
 * @file    mcpwm_driver.hpp
 * @brief   MCPWM hardware driver — Complementary PWM with dead time.
 *
 * Features:
 *   • Complementary half-bridge output (A: high-side, B: low-side)
 *   • Adjustable frequency   (20 – 100 kHz)
 *   • Adjustable duty cycle   (0 – 100 %)
 *   • Hardware dead time      (RED / FED, 0 – 1000 ns, 25 ns resolution)
 *   • Safe enable/disable with emergency stop
 *   • Glitch-free parameter updates during operation
 *
 * Dependencies:
 *   utils/numeric_utils.hpp   — constexpr limits and tick conversion
 */

#pragma once

#include <cstdint>
#include "esp_err.h"

namespace mcpwm {

/* ─── Configuration ─── */
struct Config {
    bool     enable           = false;
    uint32_t frequency_hz     = 40'000;    // 20 000 – 100 000
    float    duty_percent     = 45.0f;     //  0.0 – 100.0
    uint32_t dead_time_red_ns = 200;       //  0 – 1000  (rising edge delay)
    uint32_t dead_time_fed_ns = 200;       //  0 – 1000  (falling edge delay)
};

/* ─── Feedback (simulated / sensor-driven) ─── */
struct Feedback {
    float power_kw      = 0.0f;
    float voltage       = 0.0f;
    float current_a     = 0.0f;
    float temperature_c = 25.0f;
};

/* ════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════ */

/**
 * Initialise MCPWM timer, operator, comparator, and both generators.
 * Outputs start DISABLED — user must call set_enable(true).
 */
esp_err_t init() noexcept;

/**
 * Atomically apply a full configuration.
 * Safe to call while outputs are running.
 */
esp_err_t apply_config(const Config &cfg) noexcept;

/**
 * Read back the currently active configuration.
 */
esp_err_t get_config(Config &cfg) noexcept;

/**
 * Change frequency (Hz).  Comparator value is automatically
 * recalculated to preserve the current duty percentage.
 */
esp_err_t set_frequency(uint32_t freq_hz) noexcept;

/**
 * Change duty cycle (percent, 0.0–100.0).
 */
esp_err_t set_duty(float duty_pct) noexcept;

/**
 * Change dead time for both RED (rising edge) and FED (falling edge).
 * Each value: 0 – 1000 ns, resolution 25 ns.
 */
esp_err_t set_dead_time(uint32_t red_ns, uint32_t fed_ns) noexcept;

/**
 * Enable or disable PWM outputs.
 * Disable → timer stops, both GPIOs pulled LOW, master-enable LOW.
 */
esp_err_t set_enable(bool en) noexcept;

/**
 * Immediate emergency stop.
 * Forces outputs LOW, duty → 0%, enable → false.
 */
esp_err_t emergency_stop() noexcept;

/**
 * Read simulated feedback values (power, voltage, current, temperature).
 */
void get_feedback(Feedback &fb) noexcept;

}  // namespace mcpwm
