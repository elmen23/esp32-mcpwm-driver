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

# Link all project components
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/components)
