cmake_minimum_required(VERSION 3.16.0)

# ── Register custom components BEFORE project.cmake ──
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/components)

# ── ESP-IDF project entry point ──
# Assumes IDF_PATH is set (CI will set this)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32-mcpwm-driver)
