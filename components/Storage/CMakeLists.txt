# ── Storage Component ──
# NVS-based configuration persistence (read/write/erase)

idf_component_register(
    SRCS            ""
    INCLUDE_DIRS    "."
    REQUIRES
        nvs_flash
        esp_log
)
