# ── Web Component ──
# HTTP server, REST API endpoints, embedded web assets

idf_component_register(
    SRCS            ""
    INCLUDE_DIRS    "."
    EMBED_FILES
        index.html
        style.css
        script.js
    REQUIRES
        esp_http_server
        esp_log
)
