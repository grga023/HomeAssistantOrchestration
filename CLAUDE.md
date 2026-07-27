# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a 4-board ESP32 Smart Home demo (pure C on ESP-IDF, built with
PlatformIO). Each board talks MQTT to a Mosquitto broker on a Raspberry Pi
running Home Assistant. Entities auto-appear in HA via MQTT Discovery. Home
security (ESP4) is the emphasis. See `README.md` for wiring, MQTT topic tables,
and the security state-machine spec; `docs/*.puml` for sequence diagrams.

## Commands

First-time setup — credentials are required to compile (`mqtt_wrap.c` includes
`secrets.h`):

```
cp components/common/include/secrets.example.h components/common/include/secrets.h
# edit secrets.h: WIFI_SSID/PASSWORD, MQTT_URI, MQTT_USER/PASSWORD
```

Each board is a separate PlatformIO environment. Build / flash / monitor one
board at a time:

```
pio run -e esp1_lights                 # build
pio run -e esp4_security -t upload     # build + flash over USB
pio device monitor -e esp2_temperature # serial monitor (115200)
pio run                                # build all 4 envs (default_envs)
pio run -e esp3_garage -t clean
```

Env names: `esp1_lights`, `esp2_temperature`, `esp3_garage`, `esp4_security`.

There is no lint step wired into the build; `.vscode/settings.json` documents
the intended warning set (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`…).
The `test/` dir is a PlatformIO test placeholder — no tests exist yet.

## Build-system architecture (important, non-obvious)

All four boards live in one CMake project but produce **one firmware per board**.
Board selection flows: `platformio.ini` sets
`board_build.cmake_extra_args = -DSMARTHOME_BOARD=<env>` per env →
`src/CMakeLists.txt` `GLOB`s only `${SMARTHOME_BOARD}/src/*.c` and adds only that
board's `include/` dir. So exactly one `app_main()` is ever compiled. This glob
approach is used because ESP-IDF ignores PlatformIO's `build_src_filter`. When
you add a `.c` file under a board's `src/`, it is picked up automatically; adding
a new board means a new `src/<board>/` dir plus a new `[env:<board>]` block.

## Runtime architecture

Shared code is the `common` IDF component (`components/common/`), depended on by
every board via `REQUIRES common` in `src/CMakeLists.txt`. Four wrappers:

- **`wifi_sta`** — `wifi_sta_start(hostname)`: NVS + netif + event loop + STA
  connect, blocking.
- **`mqtt_wrap`** — `mqtt_start(board, on_msg, on_conn)`. Sets an MQTT Last-Will
  on `home/<board>/status` = `offline` (retained); publishes `online` on
  connect. `on_conn` fires on **every (re)connect**; `on_msg` dispatches
  incoming messages.
- **`ha_discovery`** — `ha_set_device(board, display_name)` then
  `ha_publish_config(component, object_id, extra_json)` publishes retained
  `homeassistant/<component>/<board>/<object_id>/config`. Availability + device
  block are injected automatically; `extra_json` is component-specific keys
  **without** braces.
- **`app_rtos`** — `rtos_every_ms(...)` spawns a drift-free periodic FreeRTOS
  task. Conventions in `app_rtos.h`: app logic on core 1 (`RTOS_CORE_APP`),
  networking on core 0; `RTOS_PRIO_HIGH` reserved for security-critical loops.

### Per-board pattern

Each board is `main.c` (pins + one `*_start()` call, then idles) plus a module
(`lights.c`, `climate.c`+`dht22.c`, `garage.c`, `security.c`). Every `*_start()`
follows the same shape:

1. init GPIO,
2. `wifi_sta_start(HOSTNAME)`,
3. `ha_set_device(BOARD, display)`,
4. `mqtt_start(BOARD, on_message, on_connect)`,
5. (if it polls hardware) `rtos_every_ms(...)` for a tick task.

`on_connect()` is where a board **(re)subscribes to its command topics and
publishes HA discovery + current state** — it must be idempotent since it runs
on every reconnect. `on_message()` matches the topic and applies the command.

### MQTT topic conventions

- App topics: `home/<board>/...` (see the table in `README.md`).
- Availability: `home/<board>/status` (retained, via LWT).
- Discovery: `homeassistant/<component>/<board>/<object_id>/config` (retained).

## Gotchas

- **Incoming MQTT `topic`/`data` are NOT null-terminated.** Callbacks receive
  explicit `topic_len`/`data_len`; always compare with lengths
  (`strncmp` + length check), never `strcmp`/`strstr` on the raw pointer. This
  is the established pattern in every `on_message`.
- Relay/siren boards are typically **active-LOW**; inversion is handled by the
  `active_low` args passed from `main.c` (`level = state ^ active_low`). Adjust
  pin numbers and the `active_low` flag to your wiring, not the logic.
- `secrets.h` is gitignored — never commit it. The per-board `sdkconfig.*` files
  are also gitignored and regenerated on build.
- The security alarm state machine (`security.c`) is driven entirely by the
  100 ms `tick_task`; delays are passed from `esp4_security/src/main.c`. Its
  state strings must stay aligned with Home Assistant's `alarm_control_panel`
  values (`disarmed`/`armed_home`/`armed_away`/`pending`/`triggered`/`arming`).

## Adding a new HA entity to a board

Do all three in the board's module: subscribe to its command topic **and**
`ha_publish_config(...)` in `on_connect()`, handle its command in `on_message()`,
and publish state (retained) whenever it changes. Follow the topic naming and
the length-based topic matching already used in that file.
