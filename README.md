# ⚡ ESP32 MCPWM Driver

[![Build](https://github.com/elmen23/esp32-mcpwm-driver/actions/workflows/build.yml/badge.svg)](https://github.com/elmen23/esp32-mcpwm-driver/actions/workflows/build.yml)

**Industrial Half-Bridge PWM Controller** built on **ESP-IDF v5.4** with **MCPWM** hardware, responsive web dashboard, and SVG waveform visualizer.

> 🎛️ **20–100 kHz** | 🔴 **RED/FED Dead Time** | 🌐 **WiFi AP Web UI** | 💾 **NVS Persistence**

---

## 📋 Features

| Category | Feature |
|----------|---------|
| **PWM** | Complementary half-bridge (GPIO 18 + GPIO 19) |
| | Adjustable frequency (20–100 kHz, 1 Hz resolution) |
| | Adjustable duty cycle (0–100%) |
| | Hardware dead time RED/FED (0–1000 ns, 25 ns/tick) |
| | Safe enable/disable with STOP_EMPTY (no partial pulses) |
| | Emergency stop — immediate shutdown |
| **WiFi** | Access Point mode (SSID: `ESP32-MCPWM`) |
| | Optional Station mode for router connection |
| **Web UI** | Industrial dark-theme dashboard |
| | Power ring gauge (SVG) |
| | 4-column PWM parameter controls (slider + numeric) |
| | SVG waveform visualizer — PWM-A, PWM-B, dead time zone |
| | AJAX polling — no page reload |
| **REST API** | `GET/POST /api/config` — read/update PWM settings |
| | `GET /api/status` — live telemetry (kW, V, A, °C) |
| | `POST /api/estop` — emergency stop |
| | CORS headers for development |
| **Storage** | NVS config persistence — settings survive power cycles |
| | Auto-save every 30 seconds |
| | Factory reset via API |
| **CI/CD** | GitHub Actions — builds on push/PR |
| | Official Espressif Docker image (`espressif/idf:v5.4`) |
| | Automatic Release on version tags |

---

## 🔌 Hardware Pinout

| Signal | GPIO | Description |
|--------|------|-------------|
| **MCPWM-A** | 18 | High-side gate driver (e.g., IR2101/IR2110) |
| **MCPWM-B** | 19 | Low-side gate driver (complementary) |
| **ENABLE** | 4 | Master enable (active HIGH, pull-down) |

```
         ┌─────────┐
  GPIO18 ─┤  High   ├──→ To half-bridge / resonant tank
          │  Side   │
  GPIO19 ─┤  Low    │
          └─────────┘
```

> ⚠️ **Dead time values must be tuned for your specific IGBT/MOSFET gate driver.** Insufficient dead time causes shoot-through and can destroy your hardware.

---

## 🚀 Quick Start

### Prerequisites

- ESP32 dev board (ESP32-DevKitC, NodeMCU-32S, etc.)
- Half-bridge gate driver (IR2101/IR2110) + IGBTs/MOSFETs
- [ESP-IDF v5.4](https://docs.espressif.com/projects/esp-idf/en/v5.4/)

### Option A: Download Pre-built Firmware

Download the latest `.bin` files from [**Releases**](https://github.com/elmen23/esp32-mcpwm-driver/releases/tag/v1.0.0):

- `esp32-mcpwm-driver.bin` → application firmware
- `bootloader.bin` → bootloader
- `partition-table.bin` → partition table

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 \
  write_flash 0x0 bootloader.bin \
  0x8000 partition-table.bin \
  0x10000 esp32-mcpwm-driver.bin
```

### Option B: Build from Source

```bash
# Clone the repository
git clone https://github.com/elmen23/esp32-mcpwm-driver.git
cd esp32-mcpwm-driver

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Usage

1. Flash the firmware
2. Connect to WiFi **`ESP32-MCPWM`** (password: `mcpwm2024`)
3. Open browser → **http://192.168.4.1**
4. Configure frequency, duty cycle, and dead time
5. Toggle **Enable** to start PWM output

---

## 🏗️ Project Structure

```
esp32-mcpwm-driver/
├── main/
│   ├── CMakeLists.txt              ← Entry point dependencies
│   └── main.cpp                    ← Init sequence + main loop
├── components/
│   ├── Utils/
│   │   └── numeric_utils.hpp       ← constexpr, clamp, tick conversion
│   ├── Drivers/
│   │   ├── mcpwm_driver.hpp        ← MCPWM public API
│   │   └── mcpwm_driver.cpp        ← PWM implementation (ESP-IDF v5.4)
│   ├── Network/
│   │   ├── wifi_ap.hpp             ← WiFi AP API
│   │   └── wifi_ap.cpp             ← NVS init, AP/STA, event handling
│   ├── Web/
│   │   ├── http_server.hpp         ← HTTP server API
│   │   ├── http_server.cpp         ← REST endpoints + embedded assets
│   │   ├── index.html              ← HMI dashboard
│   │   ├── style.css               ← Dark theme (responsive)
│   │   └── script.js               ← Sliders, SVG waveform, polling
│   └── Storage/
│       ├── nvs_config.hpp          ← Persistence API
│       └── nvs_config.cpp          ← NVS read/write/erase
├── partitions.csv                  ← Factory + 2×OTA + NVS
├── sdkconfig.defaults              ← Minimal config overrides
└── .github/workflows/build.yml     ← CI/CD pipeline
```

---

## 📡 API Reference

All endpoints return `application/json`.

### `GET /api/config`

```json
{
  "enable": false,
  "frequency": 40.0,
  "duty": 45.0,
  "dead_time_red": 200,
  "dead_time_fed": 200
}
```

> **Note:** `frequency` is in **kHz**.

### `POST /api/config`

```json
{
  "enable": true,
  "frequency": 50.0,
  "duty": 48.0,
  "dead_time_red": 150,
  "dead_time_fed": 150
}
```

All fields are optional — only provided fields are updated.

### `GET /api/status`

```json
{
  "enable": true,
  "frequency": 50.0,
  "duty": 48.0,
  "power": 8.2,
  "voltage": 230.5,
  "current": 35.7,
  "temperature": 42.0,
  "wifi_connected": false,
  "wifi_mode": "AP",
  "wifi_ip": "192.168.4.1",
  "uptime_sec": 3600.0
}
```

### `POST /api/estop`

```json
{
  "status": "ESTOPPED"
}
```

Immediately disables all PWM outputs and forces duty to 0%.

---

## 🛡️ Safety

- ⏹️ **Outputs start DISABLED** — must be explicitly enabled via web UI
- ⚡ **Emergency stop** — immediate shutdown via button or API
- 🔒 **STOP_EMPTY on disable** — waits for current cycle to prevent partial pulses
- 🕐 **Hardware dead time** — prevents shoot-through in half-bridge topology
- ✅ **All settings validated** — range-checked before applying

---

## 🔧 Development

### Code Quality

- **Modern C++** — `constexpr`, `namespace`, `enum class`, `noexcept`
- **Clean architecture** — 5 independent components, minimal coupling
- **No dead code** — zero unused functions or variables
- **Clear naming** — units in variable names (e.g., `freq_hz`, `dead_time_red_ns`)

### CI/CD

On every push to `main` or Pull Request:

1. Checkout → Build with `espressif/idf:v5.4` Docker image
2. Rename binaries → `firmware.bin`, `bootloader.bin`, `partition-table.bin`
3. Upload as artifact
4. Auto-create GitHub Release on `v*` tags

---

## 📄 License

MIT

---

<p align="center">
  <b>ESP32 MCPWM Driver v1.0.0</b><br>
  <sub>Built with ESP-IDF v5.4 · Industrial Half-Bridge PWM Controller</sub>
</p>
