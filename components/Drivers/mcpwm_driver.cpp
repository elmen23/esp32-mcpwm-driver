/**
 * @file    mcpwm_driver.cpp
 * @brief   MCPWM hardware driver implementation (ESP-IDF v5.4 API).
 *
 * Half-bridge complementary PWM:
 *   • GPIO 18  →  PWM-A (high-side)
 *   • GPIO 19  →  PWM-B (low-side)
 *   • GPIO 4   →  Master enable
 *
 * Architecture:
 *   1 Timer (40 MHz, up-counter) → 1 Operator → 1 Comparator → 2 Generators
 *   Dead time: hardware RED on Gen-A, FED on Gen-B
 */

#include "mcpwm_driver.hpp"
#include "numeric_utils.hpp"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_bit_defs.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"

static constexpr const char *TAG = "MCPWM";

/* ════════════════════════════════════════════
 *  File‑local static handles & state
 * ════════════════════════════════════════════ */

static mcpwm_timer_handle_t  s_timer  = nullptr;
static mcpwm_oper_handle_t   s_oper   = nullptr;
static mcpwm_cmpr_handle_t   s_cmpr   = nullptr;
static mcpwm_gen_handle_t    s_gen_a  = nullptr;
static mcpwm_gen_handle_t    s_gen_b  = nullptr;

static mcpwm::Config s_cfg {};       // active configuration
static mcpwm::Feedback s_fb {};      // simulated feedback state

/* ════════════════════════════════════════════
 *  Internal helpers
 * ════════════════════════════════════════════ */

/** Recalculate comparator value from current duty % and frequency. */
static void update_comparator() noexcept {
    const uint32_t cmp = utils::duty_to_compare_ticks(
        s_cfg.duty_percent, s_cfg.frequency_hz);
    mcpwm_comparator_set_compare_value(s_cmpr, cmp);
}

/** Apply dead time to a generator — RED (posedge delay) or FED (negedge delay). */
static void apply_dt_red(const mcpwm_gen_handle_t gen,
                          const uint32_t dt_ns) noexcept {
    mcpwm_dead_time_config_t dt_cfg {
        .posedge_delay_ticks = utils::ns_to_deadtime_ticks(dt_ns),
        .negedge_delay_ticks = 0,
        .flags.invert_output = false,
    };
    mcpwm_generator_set_dead_time(gen, &dt_cfg);
}

static void apply_dt_fed(const mcpwm_gen_handle_t gen,
                          const uint32_t dt_ns) noexcept {
    mcpwm_dead_time_config_t dt_cfg {
        .posedge_delay_ticks = 0,
        .negedge_delay_ticks = utils::ns_to_deadtime_ticks(dt_ns),
        .flags.invert_output = false,
    };
    mcpwm_generator_set_dead_time(gen, &dt_cfg);
}

/* ════════════════════════════════════════════
 *  Public API Implementation
 * ════════════════════════════════════════════ */

namespace mcpwm {

esp_err_t init() noexcept {
    ESP_LOGI(TAG, "Initialising MCPWM — %" PRIu32 " Hz base clock",
             utils::MCPWM_RESOLUTION_HZ);

    /* ── 1. Timer: 40 MHz, up-counter ── */
    mcpwm_timer_config_t timer_cfg {};
    timer_cfg.clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT;
    timer_cfg.resolution_hz = utils::MCPWM_RESOLUTION_HZ;
    timer_cfg.count_mode    = MCPWM_TIMER_COUNT_MODE_UP;
    timer_cfg.period_ticks  = utils::MCPWM_RESOLUTION_HZ / s_cfg.frequency_hz;
    ESP_RETURN_ON_ERROR(
        mcpwm_new_timer(&timer_cfg, &s_timer), TAG, "new_timer");

    /* ── 2. Operator ── */
    mcpwm_operator_config_t oper_cfg {};
    ESP_RETURN_ON_ERROR(
        mcpwm_new_operator(&oper_cfg, &s_oper), TAG, "new_operator");
    ESP_RETURN_ON_ERROR(
        mcpwm_operator_connect_timer(s_oper, s_timer), TAG, "connect_timer");

    /* ── 3. Comparator (update on timer zero) ── */
    mcpwm_comparator_config_t cmpr_cfg {};
    cmpr_cfg.flags.update_cmp_on_tez = true;
    ESP_RETURN_ON_ERROR(
        mcpwm_new_comparator(s_oper, &cmpr_cfg, &s_cmpr), TAG, "new_comparator");

    /* ── 4. Generator A — high‑side on GPIO 18 ── */
    mcpwm_generator_config_t gen_a_cfg {};
    ESP_RETURN_ON_ERROR(
        mcpwm_new_generator(s_oper, &gen_a_cfg, &s_gen_a), TAG, "gen_a");
    ESP_RETURN_ON_ERROR(
        mcpwm_generator_set_gpio(s_gen_a, utils::GPIO_PWM_A,
                                 MCPWM_GEN_GPIO_TYPE_SIGNAL),
        TAG, "gen_a_gpio");

    /* ── 5. Generator B — low‑side on GPIO 19 ── */
    mcpwm_generator_config_t gen_b_cfg {};
    ESP_RETURN_ON_ERROR(
        mcpwm_new_generator(s_oper, &gen_b_cfg, &s_gen_b), TAG, "gen_b");
    ESP_RETURN_ON_ERROR(
        mcpwm_generator_set_gpio(s_gen_b, utils::GPIO_PWM_B,
                                 MCPWM_GEN_GPIO_TYPE_SIGNAL),
        TAG, "gen_b_gpio");

    /* ── 6. Actions: Gen-A HIGH on timer=0, LOW on compare ── */
    {
        mcpwm_gen_timer_event_action_t ta = {
            .direction = MCPWM_TIMER_DIRECTION_UP,
            .event     = MCPWM_TIMER_EVENT_EMPTY,
            .action    = MCPWM_GEN_ACTION_HIGH,
        };
        ESP_RETURN_ON_ERROR(
            mcpwm_generator_set_action_on_timer_event(s_gen_a, ta),
            TAG, "gen_a timer");
    }
    {
        mcpwm_gen_compare_event_action_t ca = {
            .direction  = MCPWM_TIMER_DIRECTION_UP,
            .comparator = s_cmpr,
            .action     = MCPWM_GEN_ACTION_LOW,
        };
        ESP_RETURN_ON_ERROR(
            mcpwm_generator_set_action_on_compare_event(s_gen_a, ca),
            TAG, "gen_a compare");
    }

    /* ── 7. Actions: Gen-B LOW on timer=0, HIGH on compare (complementary) ── */
    {
        mcpwm_gen_timer_event_action_t ta = {
            .direction = MCPWM_TIMER_DIRECTION_UP,
            .event     = MCPWM_TIMER_EVENT_EMPTY,
            .action    = MCPWM_GEN_ACTION_LOW,
        };
        ESP_RETURN_ON_ERROR(
            mcpwm_generator_set_action_on_timer_event(s_gen_b, ta),
            TAG, "gen_b timer");
    }
    {
        mcpwm_gen_compare_event_action_t ca = {
            .direction  = MCPWM_TIMER_DIRECTION_UP,
            .comparator = s_cmpr,
            .action     = MCPWM_GEN_ACTION_HIGH,
        };
        ESP_RETURN_ON_ERROR(
            mcpwm_generator_set_action_on_compare_event(s_gen_b, ca),
            TAG, "gen_b compare");
    }

    /* ── 8. Dead time: RED (posedge) on Gen‑A, FED (negedge) on Gen‑B ── */
    apply_dt_red(s_gen_a, s_cfg.dead_time_red_ns);
    apply_dt_fed(s_gen_b, s_cfg.dead_time_fed_ns);

    /* ── 9. Set initial comparator ── */
    update_comparator();

    /* ── 10. Enable timer and start with outputs OFF ── */
    ESP_RETURN_ON_ERROR(
        mcpwm_timer_enable(s_timer), TAG, "timer_enable");
    ESP_RETURN_ON_ERROR(
        mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_STOP_FULL),
        TAG, "timer_stop");

    /* ── 11. Master enable GPIO (active HIGH, pull‑down) ── */
    gpio_config_t en_gpio {};
    en_gpio.pin_bit_mask = BIT64(utils::GPIO_ENABLE);
    en_gpio.mode         = GPIO_MODE_OUTPUT;
    en_gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&en_gpio);
    gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_ENABLE), 0);

    ESP_LOGI(TAG, "Ready — A:GPIO%d  B:GPIO%d  EN:GPIO%d  |  %" PRIu32 " kHz  %.1f %%  RED:%" PRIu32 "  FED:%" PRIu32,
             utils::GPIO_PWM_A, utils::GPIO_PWM_B, utils::GPIO_ENABLE,
             s_cfg.frequency_hz / 1000, static_cast<double>(s_cfg.duty_percent),
             s_cfg.dead_time_red_ns, s_cfg.dead_time_fed_ns);
    return ESP_OK;
}

/* ────────────────────────────────────────────
 *  apply_config  —  full atomic update
 * ──────────────────────────────────────────── */

esp_err_t apply_config(const Config &cfg) noexcept {
    ESP_RETURN_ON_ERROR(set_frequency(cfg.frequency_hz),   TAG, "freq");
    ESP_RETURN_ON_ERROR(set_duty(cfg.duty_percent),        TAG, "duty");
    ESP_RETURN_ON_ERROR(
        set_dead_time(cfg.dead_time_red_ns, cfg.dead_time_fed_ns), TAG, "dt");
    ESP_RETURN_ON_ERROR(set_enable(cfg.enable),             TAG, "enable");

    s_cfg = cfg;
    ESP_LOGI(TAG, "Applied: %" PRIu32 " kHz  %.1f %%  RED:%" PRIu32 "  FED:%" PRIu32 "  %s",
             cfg.frequency_hz / 1000, static_cast<double>(cfg.duty_percent),
             cfg.dead_time_red_ns, cfg.dead_time_fed_ns,
             cfg.enable ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t get_config(Config &cfg) noexcept {
    cfg = s_cfg;
    return ESP_OK;
}

/* ────────────────────────────────────────────
 *  set_frequency  —  preserves duty %
 *  Uses mcpwm_timer_reconfigure() for glitch-free
 *  runtime frequency changes.
 * ──────────────────────────────────────────── */

esp_err_t set_frequency(const uint32_t freq_hz) noexcept {
    if (freq_hz < utils::FREQ_HZ_MIN || freq_hz > utils::FREQ_HZ_MAX) {
        ESP_LOGW(TAG, "Frequency %" PRIu32 " out of range", freq_hz);
        return ESP_ERR_INVALID_ARG;
    }

    s_cfg.frequency_hz = freq_hz;

    /* ── Set new timer period (glitch-free at next TEZ) ── */
    const uint32_t period_ticks = utils::MCPWM_RESOLUTION_HZ / freq_hz;
    ESP_RETURN_ON_ERROR(
        mcpwm_timer_set_period(s_timer, period_ticks),
        TAG, "timer_set_period");

    /* ── Recalculate comparator so duty % stays correct ── */
    update_comparator();

    ESP_LOGI(TAG, "Frequency updated: %" PRIu32 " kHz  (period=%" PRIu32 " ticks)",
             freq_hz / 1000, reconfig.period_ticks);
    return ESP_OK;
}

/* ────────────────────────────────────────────
 *  set_duty
 * ──────────────────────────────────────────── */

esp_err_t set_duty(const float duty_pct) noexcept {
    if (duty_pct < utils::DUTY_PCT_MIN || duty_pct > utils::DUTY_PCT_MAX) {
        ESP_LOGW(TAG, "Duty %.1f out of range", static_cast<double>(duty_pct));
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg.duty_percent = duty_pct;
    update_comparator();
    return ESP_OK;
}

/* ────────────────────────────────────────────
 *  set_dead_time
 * ──────────────────────────────────────────── */

esp_err_t set_dead_time(const uint32_t red_ns,
                         const uint32_t fed_ns) noexcept {
    if (red_ns > utils::DEADTIME_NS_MAX || fed_ns > utils::DEADTIME_NS_MAX) {
        ESP_LOGW(TAG, "Dead time out of range");
        return ESP_ERR_INVALID_ARG;
    }

    apply_dt_red(s_gen_a, red_ns);
    apply_dt_fed(s_gen_b, fed_ns);

    s_cfg.dead_time_red_ns = red_ns;
    s_cfg.dead_time_fed_ns = fed_ns;
    return ESP_OK;
}

/* ────────────────────────────────────────────
 *  set_enable
 * ──────────────────────────────────────────── */

esp_err_t set_enable(const bool en) noexcept {
    if (en) {
        ESP_RETURN_ON_ERROR(
            mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP),
            TAG, "timer_start");
        gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_ENABLE), 1);
    } else {
        // Stop at next timer empty — prevents partial-pulse glitch
        ESP_RETURN_ON_ERROR(
            mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_STOP_EMPTY),
            TAG, "timer_stop");
        gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_ENABLE), 0);
        gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_PWM_A), 0);
        gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_PWM_B), 0);
    }
    s_cfg.enable = en;
    return ESP_OK;
}

/* ────────────────────────────────────────────
 *  emergency_stop
 * ──────────────────────────────────────────── */

esp_err_t emergency_stop() noexcept {
    ESP_LOGW(TAG, "*** EMERGENCY STOP ***");

    // Immediate stop — no waiting for cycle end (safety first)
    mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_STOP_FULL);
    gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_ENABLE), 0);
    gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_PWM_A), 0);
    gpio_set_level(static_cast<gpio_num_t>(utils::GPIO_PWM_B), 0);

    s_cfg.enable       = false;
    s_cfg.duty_percent = 0.0f;

    return ESP_OK;
}

/* ────────────────────────────────────────────
 *  get_feedback  —  simulated telemetry
 * ──────────────────────────────────────────── */

void get_feedback(Feedback &fb) noexcept {
    if (s_cfg.enable) {
        const float freq_khz = static_cast<float>(s_cfg.frequency_hz) / 1000.0f;
        const float pct      = s_cfg.duty_percent / 100.0f;

        s_fb.power_kw  = 15.0f * pct * (freq_khz / 40.0f);
        s_fb.voltage   = 220.0f + (s_fb.power_kw * 1.2f);
        s_fb.current_a = (s_fb.power_kw * 1000.0f) / (s_fb.voltage + 1.0f);

        s_fb.temperature_c += (s_fb.power_kw * 0.002f) - 0.05f;
        if (s_fb.temperature_c < 25.0f)  s_fb.temperature_c = 25.0f;
        if (s_fb.temperature_c > 110.0f) s_fb.temperature_c = 110.0f;
    } else {
        s_fb.power_kw  = 0.0f;
        s_fb.voltage   = 0.0f;
        s_fb.current_a = 0.0f;
        if (s_fb.temperature_c > 25.0f) s_fb.temperature_c -= 0.1f;
    }
    fb = s_fb;
}

}  // namespace mcpwm
