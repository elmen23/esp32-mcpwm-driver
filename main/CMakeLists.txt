# ── Main component ──
idf_component_register(
    SRCS            "main.cpp"
    INCLUDE_DIRS    "."
    REQUIRES
        esp_wifi
        esp_http_server
        nvs_flash
        driver
        esp_timer
        Drivers
        Network
        Web
        Storage
)
