# ── Network Component ──
# WiFi AP mode, optional STA mode, connection management

idf_component_register(
    SRCS            ""
    INCLUDE_DIRS    "."
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        esp_log
)
