# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a 2-board ESP32 Smart Home demo (pure C on ESP-IDF, built with
PlatformIO). Each board talks MQTT to a Mosquitto broker on a Raspberry Pi
running Home Assistant. Entities auto-appear in HA via MQTT Discovery. Firmware
can be updated **over the air** (OTA) from GitHub Releases. See `README.md` for
wiring and MQTT topic tables, `docs/*.puml` for sequence diagrams, and
`docs/measurement/` for the plain-MQTT-vs-TLS measurement study.

## Commands

First-time setup — credentials are required to compile (`mqtt_wrap.c` includes
`secrets.h`):

```
cp components/common/include/secrets.example.h components/common/include/secrets.h
# edit secrets.h: WIFI_SSID/PASSWORD, MQTT_URI, MQTT_USER/PASSWORD
```

OTA is wired to `grga023/HomeAssistantOrchestration` releases via the
`-DOTA_MANIFEST_URL_BASE=...` build flag in `platformio.ini` (change the slug if
you fork).

Each board is a separate PlatformIO environment. Build / flash / monitor one
board at a time:

```
pio run -e esp1_lights                 # build
pio run -e esp2_temperature -t upload  # build + flash over USB
pio device monitor -e esp2_temperature # serial monitor (115200)
pio run                                # build both envs (default_envs)
pio run -e esp1_lights -t clean
```

Env names: `esp1_lights`, `esp2_temperature`.

There is no lint step wired into the build; `.vscode/settings.json` documents
the intended warning set (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`…).
The `test/` dir is a PlatformIO test placeholder — no tests exist yet.

## Build-system architecture (important, non-obvious)

Both boards live in one CMake project but produce **one firmware per board**.
Board selection flows: `platformio.ini` sets
`board_build.cmake_extra_args = -DSMARTHOME_BOARD=<env>` per env →
`src/CMakeLists.txt` `GLOB`s only `${SMARTHOME_BOARD}/src/*.c` and adds only that
board's `include/` dir. So exactly one `app_main()` is ever compiled. This glob
approach is used because ESP-IDF ignores PlatformIO's `build_src_filter`. When
you add a `.c` file under a board's `src/`, it is picked up automatically; adding
a new board means a new `src/<board>/` dir plus a new `[env:<board>]` block.

Flash/partition config is shared in `sdkconfig.defaults` + `partitions.csv`
(dual-OTA, 4 MB). The per-board `sdkconfig.*` files are gitignored and
regenerated from those defaults on build — edit `sdkconfig.defaults`, not the
generated files.

## Runtime architecture

Shared code is the `common` IDF component (`components/common/`), depended on by
every board via `REQUIRES common` in `src/CMakeLists.txt`. Five wrappers:

- **`wifi_sta`** — `wifi_sta_start(hostname)`: NVS + netif + event loop + STA
  connect, blocking.
- **`mqtt_wrap`** — `mqtt_start(board, on_msg, on_conn)`. Sets an MQTT Last-Will
  on `home/<board>/status` = `offline` (retained); publishes `online` on
  connect. `on_conn` fires on **every (re)connect**; `on_msg` dispatches
  incoming messages. Also drives OTA centrally (see below).
- **`ha_discovery`** — `ha_set_device(board, display_name)` then
  `ha_publish_config(component, object_id, extra_json)` publishes retained
  `homeassistant/<component>/<board>/<object_id>/config`. Availability + device
  block (incl. `sw_version` from the app descriptor) are injected automatically;
  `extra_json` is component-specific keys **without** braces.
- **`app_rtos`** — `rtos_every_ms(...)` spawns a drift-free periodic FreeRTOS
  task. Conventions in `app_rtos.h`: app logic on core 1 (`RTOS_CORE_APP`),
  networking on core 0 (`RTOS_CORE_NET`).
- **`ota`** — MQTT-triggered OTA. `ota_start(board)`, `ota_on_connect()`,
  `ota_handle_message(...)`, `ota_confirm_running_image()`. Wired into
  `mqtt_wrap` so it applies to every board with no per-module code.

### Per-board pattern

Each board is `main.c` (pins + one `*_start()` call, then idles) plus a module
(`lights.c`, `climate.c`+`dht22.c`). Every `*_start()` follows the same shape:

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
- OTA: `home/<board>/ota/{set,install,state}`.

## OTA (over-the-air updates)

Handled centrally in `components/common` — boards need no OTA code. `mqtt_wrap`,
on each connect, calls `ota_confirm_running_image()` (marks the running image
valid → cancels rollback once MQTT works) then `ota_on_connect()` (subscribes,
publishes the HA `update` entity, fetches the release manifest). Incoming OTA
topics are intercepted by `ota_handle_message()` before the board callback.

Transport: `esp_https_ota` over HTTPS using the public CA bundle
(`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE`). Binaries + a per-board `<board>.json`
manifest (`{version,url}`) are hosted on GitHub Releases; the base URL is the
`OTA_MANIFEST_URL_BASE` build flag. Version comes from `version.txt` via
`esp_app_get_description()`. See `README.md` for the release process.

## Gotchas

- **Incoming MQTT `topic`/`data` are NOT null-terminated.** Callbacks receive
  explicit `topic_len`/`data_len`; always compare with lengths
  (`strncmp` + length check), never `strcmp`/`strstr` on the raw pointer. This
  is the established pattern in every `on_message` and in `ota_handle_message`.
- Relay boards are typically **active-LOW**; inversion is handled by the
  `active_low` args passed from `main.c` (`level = state ^ active_low`). Adjust
  pin numbers and the `active_low` flag to your wiring, not the logic.
- `secrets.h` is gitignored — never commit it. The per-board `sdkconfig.*` files
  are also gitignored and regenerated on build (edit `sdkconfig.defaults`).
- OTA requires **4 MB flash** and cannot bootstrap itself — the first image must
  be flashed over USB.

## Adding a new HA entity to a board

Do all three in the board's module: subscribe to its command topic **and**
`ha_publish_config(...)` in `on_connect()`, handle its command in `on_message()`,
and publish state (retained) whenever it changes. Follow the topic naming and
the length-based topic matching already used in that file.

## Embedded agent team

This repo ships a persistent team of specialist subagents under `.claude/agents/`
(see `.claude/agents/README.md` for roster + orchestration). Roles:
`firmware-dev`, `mqtt-ha-dev`, `ota-build-dev` (developers); `unit-tester`,
`integration-tester` (test); `qa-reviewer`, `build-qa` (QA). The main session acts
as tech-lead: split work into a shared task list, spawn the relevant specialists
(multiple instances when work fans out), let them coordinate, then integrate.
Standard pipeline: implement → `build-qa` → tests → `qa-reviewer`.
