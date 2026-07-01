# ── Core Component ──
# System initialization, configuration dispatch, main app state

idf_component_register(
    SRCS            ""
    INCLUDE_DIRS    "."
    REQUIRES
        esp_system
        esp_log
        freertos
)
