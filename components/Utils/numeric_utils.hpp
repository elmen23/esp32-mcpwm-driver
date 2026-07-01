/**
 * @file    numeric_utils.hpp
 * @brief   Constexpr constants, range clamping, and MCPWM tick conversions.
 *
 * All functions are `inline` to allow header-only usage with zero overhead.
 * This component has NO runtime state — purely utility math.
 */

#pragma once

#include <cstdint>
#include <algorithm>

namespace utils {

/* ─── MCPWM Hardware Constants ─── */
inline constexpr uint32_t MCPWM_RESOLUTION_HZ  = 40'000'000;   // 40 MHz timer base
inline constexpr uint32_t NS_PER_TICK           = 25;          // 40 MHz → 25 ns/tick
inline constexpr uint32_t DEADTIME_TICKS_MAX    = 255;         // 8-bit dead time register

/* ─── PWM Operating Limits ─── */
inline constexpr uint32_t FREQ_HZ_MIN    = 20'000;    //  20 kHz
inline constexpr uint32_t FREQ_HZ_MAX    = 100'000;   // 100 kHz
inline constexpr float    DUTY_PCT_MIN   = 0.0f;      //   0 %
inline constexpr float    DUTY_PCT_MAX   = 100.0f;    // 100 %
inline constexpr uint32_t DEADTIME_NS_MIN = 0;        //   0 ns
inline constexpr uint32_t DEADTIME_NS_MAX = 1000;     // 1000 ns

/* ─── GPIO Pin Assignments ─── */
inline constexpr int GPIO_PWM_A  = 18;   // High-side / PWM-A output
inline constexpr int GPIO_PWM_B  = 19;   // Low-side  / PWM-B complementary
inline constexpr int GPIO_ENABLE = 4;    // Master enable (shutdown all outputs)

/* ════════════════════════════════════════════
 *  Inline Helpers
 * ════════════════════════════════════════════ */

/**
 * Clamp a value between `lo` and `hi` (inclusive).
 */
template <typename T>
inline constexpr T clamp(T val, T lo, T hi) noexcept {
    return std::max(lo, std::min(val, hi));
}

/**
 * Convert nanoseconds → MCPWM dead-time ticks (0–255).
 */
inline constexpr uint32_t ns_to_deadtime_ticks(const uint32_t ns) noexcept {
    const uint32_t ticks = ns / NS_PER_TICK;
    return (ticks > DEADTIME_TICKS_MAX) ? DEADTIME_TICKS_MAX : ticks;
}

/**
 * Convert duty percentage + frequency → MCPWM comparator tick value.
 * @param duty_pct  0.0 – 100.0
 * @param freq_hz   current timer frequency in Hz
 */
inline constexpr uint32_t duty_to_compare_ticks(const float duty_pct,
                                                 const uint32_t freq_hz) noexcept {
    const float fraction = utils::clamp(duty_pct, 0.0f, 100.0f) / 100.0f;
    return static_cast<uint32_t>(fraction * MCPWM_RESOLUTION_HZ / freq_hz);
}

}  // namespace utils
