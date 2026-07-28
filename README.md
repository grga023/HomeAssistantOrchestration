# HomeAssistantOrchestration

A demo **Smart Home** built around **Home Assistant** (on a Raspberry Pi) and
**2x ESP32** controllers talking to it over **MQTT**. Firmware is **pure C on
ESP-IDF**, built with **PlatformIO**. Supports **OTA firmware updates** from
GitHub Releases, surfaced as Home Assistant `update` entities.

```
                 +-------------------------+
                 |  Raspberry Pi           |
                 |  Home Assistant + MQTT  |
                 +-----------+-------------+
                             | MQTT (Mosquitto)
              +--------------+--------------+
              |                             |
          ESP1 Lights                  ESP2 Temp
          relay board                  DHT22 + heater
```

## Boards

| Board | Env | Role | Hardware | HA entities |
|-------|-----|------|----------|-------------|
| ESP1 | `esp1_lights` | Lighting | 2-4 ch relay board | `switch` x N, `update` |
| ESP2 | `esp2_temperature` | Climate | DHT22 + optional heater relay | `sensor` (temp, humidity), `switch` (heater), `update` |

Entities appear in Home Assistant automatically via **MQTT Discovery**. Each
board reports availability through an MQTT **Last Will** (online/offline) and its
running firmware version (`sw_version`) in the HA device block.

> These two boards are also the subjects of the measurement study in
> `docs/measurement/` (ESP1 = plain MQTT / 1883, ESP2 = MQTTS-TLS / 8883).

## Stack

- **ESP-IDF** (pure C), **PlatformIO** (`framework = espidf`)
- **esp-mqtt** for MQTT, **esp_wifi** (station), **cJSON**, native **FreeRTOS**
- **OTA**: `esp_https_ota` over HTTPS (public CA bundle), dual-app partitions +
  rollback

## Repository layout

```
platformio.ini                2 envs; board selected via cmake_extra_args
partitions.csv                dual-OTA table (ota_0/ota_1/otadata), 4 MB
sdkconfig.defaults            shared IDF config (4 MB, OTA, rollback, CA bundle)
version.txt                   firmware version -> esp_app_desc / HA sw_version
src/
  CMakeLists.txt              picks the active board (-DSMARTHOME_BOARD=...)
  esp1_lights/{include,src}   lights.c        + main.c
  esp2_temperature/{...}      climate.c, dht22.c + main.c
components/common/            shared IDF component
  include/ + src/             wifi_sta, mqtt_wrap, ha_discovery, app_rtos, ota
  include/secrets.example.h   copy -> secrets.h (gitignored)
docs/                         PlantUML sequence diagrams (per board + boot)
docs/measurement/             MQTT plain-vs-TLS measurement study (AsciiDoc)
```

Board selection: `platformio.ini` passes `-DSMARTHOME_BOARD=<env>` to CMake and
`src/CMakeLists.txt` compiles only that board's sources (so exactly one
`app_main`). ESP-IDF ignores `build_src_filter`, hence this approach.

## Prerequisites

- [PlatformIO](https://platformio.org/) (installs the ESP-IDF toolchain on first build)
- Raspberry Pi with Home Assistant + **Mosquitto** broker + **MQTT integration**
- ESP32 modules with **4 MB flash** (standard WROOM-32) — required for dual-OTA

## Setup

1. Provide credentials:
   ```
   cp components/common/include/secrets.example.h components/common/include/secrets.h
   # edit secrets.h: WiFi SSID/pass, MQTT_URI, MQTT_USER/PASSWORD
   ```
2. OTA points at `grga023/HomeAssistantOrchestration` releases by default; if
   you fork, update the repo slug in the `build_flags`
   `-DOTA_MANIFEST_URL_BASE=...` line of `platformio.ini`.
3. Build a board:
   ```
   pio run -e esp1_lights
   pio run -e esp2_temperature
   ```
4. First flash over USB (OTA can't bootstrap itself):
   ```
   pio run -e esp1_lights -t upload
   ```

## MQTT topics

Availability (retained, via LWT): `home/<board>/status` -> `online` / `offline`

| Function | Command topic | State topic |
|----------|---------------|-------------|
| Light N | `home/lights/light<N>/set` | `home/lights/light<N>/state` |
| Temp/humidity | - | `home/temperature/sensor/state` (JSON) |
| Heater | `home/temperature/heater/set` | `home/temperature/heater/state` |
| OTA (per board) | `home/<board>/ota/set` (`.bin` URL), `home/<board>/ota/install` (Install) | `home/<board>/ota/state` (JSON: installed/latest version) |

## OTA firmware updates

Updates are triggered over MQTT and downloaded over **HTTPS from GitHub
Releases**, verified with the ESP-IDF **public certificate bundle** (no embedded
cert, no local server). The partition table is dual-app (`ota_0`/`ota_1`) with
**bootloader rollback**: a freshly-flashed image is only marked valid after it
successfully reconnects to MQTT, so a broken build automatically reverts on the
next reboot.

Home Assistant shows an `update` entity per board (installed vs latest version +
an Install button). OTA is wired centrally in `components/common` (mqtt_wrap +
ota), so it applies to every board with no per-module code.

### Release process

1. Bump `version.txt` (e.g. `1.0.1`) and `pio run`.
2. Create a GitHub Release and upload, per board:
   - the binary `.pio/build/<env>/firmware.bin` as **`<board>.bin`**
     (`lights.bin`, `temperature.bin`),
   - a manifest **`<board>.json`**: `{"version":"1.0.1","url":"https://github.com/grga023/HomeAssistantOrchestration/releases/latest/download/<board>.bin"}`.
3. On the next MQTT reconnect each board fetches `<base>/<board>.json`, learns the
   latest version, and HA offers the update. Click **Install** (or publish a URL
   to `home/<board>/ota/set`) to update.

> `<board>` is the short id used in topics: `lights`, `temperature`.

## Diagrams

See `docs/*.puml` (PlantUML sequence diagrams) for the boot flow and each board.

## Roadmap

- [x] OTA updates (esp_https_ota pull over HTTPS, GitHub Releases)
- [x] MQTTS/TLS for the MQTT client itself (wired into `mqtt_wrap.c`, enabled on
  `esp2_temperature` via `-DMQTT_TLS=1`; embedded broker CA in
  `components/common/certs/ca.crt`). See `docs/measurement/` for the study.
- [ ] Per-board `sdkconfig.defaults` overrides if boards diverge

## Wiring notes

- Relay boards are usually **active-LOW**; the `active_low` args in each
  `main.c` handle inversion. Adjust pin numbers to your wiring.
- Share a common GND between the ESP32 and relay/sensor supply.

> WARNING: This is a demo, not a production-hardened system.
