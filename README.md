# HomeAssistantOrchestration

A demo **Smart Home** built around **Home Assistant** (on a Raspberry Pi) and
**4x ESP32** controllers talking to it over **MQTT**. Firmware is **pure C on
ESP-IDF**, built with **PlatformIO**. Special emphasis on **home security**.

```
                 +-------------------------+
                 |  Raspberry Pi           |
                 |  Home Assistant + MQTT  |
                 +-----------+-------------+
                             | MQTT (Mosquitto)
     +--------------+--------+--------+--------------+
     |              |                 |              |
  ESP1 Lights   ESP2 Temp        ESP3 Garage    ESP4 Security
  relay board   DHT22+heater     relay+reed     door+PIR+siren
```

## Boards

| Board | Env | Role | Hardware | HA entities |
|-------|-----|------|----------|-------------|
| ESP1 | `esp1_lights` | Lighting | 2-4 ch relay board | `switch` x N |
| ESP2 | `esp2_temperature` | Climate | DHT22 + optional heater relay | `sensor` (temp, humidity), `switch` (heater) |
| ESP3 | `esp3_garage` | Garage door | 1 relay (pulse) + reed switch | `cover` |
| ESP4 | `esp4_security` | **Security** | door reed + PIR + siren | `alarm_control_panel`, `binary_sensor` x 2 |

Entities appear in Home Assistant automatically via **MQTT Discovery**. Each
board reports availability through an MQTT **Last Will** (online/offline).

## Stack

- **ESP-IDF** (pure C), **PlatformIO** (`framework = espidf`)
- **esp-mqtt** for MQTT, **esp_wifi** (station), **cJSON**, native **FreeRTOS**
- OTA: **not yet implemented** (planned; see Roadmap)

## Repository layout

```
platformio.ini                4 envs; board selected via cmake_extra_args
src/
  CMakeLists.txt              picks the active board (-DSMARTHOME_BOARD=...)
  esp1_lights/{include,src}   lights.c        + main.c
  esp2_temperature/{...}      climate.c, dht22.c + main.c
  esp3_garage/{...}           garage.c        + main.c
  esp4_security/{...}         security.c      + main.c
components/common/            shared IDF component
  include/ + src/             wifi_sta, mqtt_wrap, ha_discovery, app_rtos
  include/secrets.example.h   copy -> secrets.h (gitignored)
docs/                         PlantUML sequence diagrams (per board + boot)
```

Board selection: `platformio.ini` passes `-DSMARTHOME_BOARD=<env>` to CMake and
`src/CMakeLists.txt` compiles only that board's sources (so exactly one
`app_main`). ESP-IDF ignores `build_src_filter`, hence this approach.

## Prerequisites

- [PlatformIO](https://platformio.org/) (installs the ESP-IDF toolchain on first build)
- Raspberry Pi with Home Assistant + **Mosquitto** broker + **MQTT integration**

## Setup

1. Provide credentials:
   ```
   cp components/common/include/secrets.example.h components/common/include/secrets.h
   # edit secrets.h: WiFi SSID/pass, MQTT_URI, MQTT_USER/PASSWORD
   ```
2. Build a board:
   ```
   pio run -e esp1_lights
   pio run -e esp2_temperature
   pio run -e esp3_garage
   pio run -e esp4_security
   ```
3. Flash (once boards are connected over USB):
   ```
   pio run -e esp4_security -t upload
   ```

## MQTT topics

Availability (retained, via LWT): `home/<board>/status` -> `online` / `offline`

| Function | Command topic | State topic |
|----------|---------------|-------------|
| Light N | `home/lights/light<N>/set` | `home/lights/light<N>/state` |
| Temp/humidity | - | `home/temperature/sensor/state` (JSON) |
| Heater | `home/temperature/heater/set` | `home/temperature/heater/state` |
| Garage door | `home/garage/door/set` (OPEN/CLOSE/STOP) | `home/garage/door/state` |
| Alarm | `home/security/alarm/set` (ARM_HOME/ARM_AWAY/DISARM) | `home/security/alarm/state` |
| Entry door | - | `home/security/door` |
| Motion | - | `home/security/motion` |

## Security behaviour (ESP4)

`alarm_control_panel` state machine:

- **DISARMED** - sensors reported, siren off.
- **ARM_AWAY** - exit delay, then armed; any door/motion -> **PENDING** -> entry
  delay -> **TRIGGERED** (siren on).
- **ARM_HOME** - perimeter only: door triggers, interior motion ignored.
- **DISARM** - clears the alarm at any time.

Delays are configurable in `src/esp4_security/src/main.c`.

## Diagrams

See `docs/*.puml` (PlantUML sequence diagrams) for the boot flow and each board.

## Roadmap

- [ ] OTA updates (esp_https_ota pull, or custom) - deferred
- [ ] Per-board `sdkconfig.defaults` (flash size, etc.)
- [ ] Home Assistant example automations for the security flow

## Wiring notes

- Relay boards are usually **active-LOW**; the `active_low` args in each
  `main.c` handle inversion. Adjust pin numbers to your wiring.
- Reed/PIR inputs use internal pull-ups; wire the switch between pin and GND.
- Share a common GND between the ESP32 and relay/sensor supply.

> WARNING: This is a demo. Do not rely on it as your only line of defense for a
> real security system.
