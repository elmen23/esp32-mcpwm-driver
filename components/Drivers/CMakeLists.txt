# ── Drivers Component ──
# MCPWM hardware driver: frequency, duty cycle, dead time, complementary outputs

idf_component_register(
    SRCS            ""
    INCLUDE_DIRS    "."
    REQUIRES
        driver
        esp_log
)
