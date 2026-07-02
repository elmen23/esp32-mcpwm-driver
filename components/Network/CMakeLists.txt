# ── Network Component ──
# WiFi AP mode, optional STA mode, NVS init, connection management

idf_component_register(
    SRCS            "wifi_ap.cpp"
    INCLUDE_DIRS    "."
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        log
)
