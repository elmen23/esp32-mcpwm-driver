cmake_minimum_required(VERSION 3.16.0)

# ── ESP-IDF project entry point ──
# Assumes IDF_PATH is set (CI will set this)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32-mcpwm-driver)
