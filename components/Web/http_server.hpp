/**
 * @file    http_server.hpp
 * @brief   HTTP server — REST API + embedded web assets.
 *
 * Endpoints:
 *   GET  /              → index.html
 *   GET  /style.css     → stylesheet
 *   GET  /script.js     → client-side logic
 *   GET  /api/config    → read MCPWM config (JSON)
 *   POST /api/config    → update MCPWM config (JSON)
 *   GET  /api/status    → status + feedback + WiFi info
 *   POST /api/estop     → emergency stop
 *
 * All API responses are application/json.
 * CORS headers allow cross-origin access for development.
 */

#pragma once

#include "esp_err.h"

namespace web {

/**
 * Start the HTTP server on port 80.
 * Registers all URI handlers and returns immediately.
 * Must be called after WiFi is initialised.
 */
esp_err_t init() noexcept;

/**
 * Gracefully stop the HTTP server.
 */
void stop() noexcept;

}  // namespace web
