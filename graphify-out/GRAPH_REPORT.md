# Graph Report - HomeAssistantOrchestration  (2026-07-28)

## Corpus Check
- 37 files · ~12,908 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 185 nodes · 257 edges · 29 communities (27 shown, 2 thin omitted)
- Extraction: 82% EXTRACTED · 18% INFERRED · 0% AMBIGUOUS · INFERRED: 46 edges (avg confidence: 0.81)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `0c1b858f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- ha_publish_config
- ota.c
- climate_start
- plot_results.py
- rtt.py
- mqtt_wrap.c
- rtos_every_ms
- HomeAssistantOrchestration — Dokumentacija implementacije
- wifi_sta.c
- mqtt-ha-dev.md
- gen-certs.sh
- Embedded agent team — HomeAssistantOrchestration
- 4. Zajednička komponenta `common`
- firmware-dev.md
- ota-build-dev.md
- certs/ — privatni CA za verifikaciju Mosquitto brokera
- build-qa.md
- integration-tester.md
- qa-reviewer.md
- unit-tester.md
- esp_event_base_t

## God Nodes (most connected - your core abstractions)
1. `OTA firmware update` - 14 edges
2. `HomeAssistantOrchestration — Dokumentacija implementacije` - 13 edges
3. `mqtt_publish()` - 10 edges
4. `mqtt_start()` - 9 edges
5. `plain-MQTT vs TLS measurement study` - 8 edges
6. `ha_publish_config()` - 7 edges
7. `mqtt_event_handler()` - 7 edges
8. `ota_on_connect()` - 7 edges
9. `climate_start()` - 7 edges
10. `bar2()` - 7 edges

## Surprising Connections (you probably didn't know these)
- `esp_https_ota over HTTPS (CA bundle)` --references--> `start_ota()`  [INFERRED]
  README.md → components/common/src/ota.c
- `plain-MQTT vs TLS measurement study` --references--> `Fig: end-to-end command RTT`  [INFERRED]
  README.md → docs/measurement/fig-rtt.png
- `Last Will availability` --references--> `mqtt_start()`  [INFERRED]
  README.md → components/common/src/mqtt_wrap.c
- `OTA firmware update` --references--> `fetch_manifest()`  [INFERRED]
  README.md → components/common/src/ota.c
- `Fig: connection time / TLS handshake` --conceptually_related_to--> `esp_https_ota over HTTPS (CA bundle)`  [INFERRED]
  docs/measurement/fig-handshake.png → README.md

## Import Cycles
- None detected.

## Communities (29 total, 2 thin omitted)

### Community 0 - "ha_publish_config"
Cohesion: 0.22
Nodes (13): ha_publish_config(), ha_set_device(), mqtt_subscribe(), ESP1 lights board, MQTT Discovery, apply_relay(), cmd_topic(), lights_start() (+5 more)

### Community 1 - "ota.c"
Cohesion: 0.25
Nodes (15): mqtt_publish(), fetch_manifest(), installed_version(), ota_confirm_running_image(), ota_handle_message(), ota_on_connect(), ota_start(), ota_task() (+7 more)

### Community 2 - "climate_start"
Cohesion: 0.20
Nodes (11): mqtt_is_connected(), esp_err_t, gpio_num_t, ESP2 temperature board, apply_heater(), climate_start(), on_message(), poll_task() (+3 more)

### Community 3 - "plot_results.py"
Cohesion: 0.24
Nodes (16): Fig: connection time / TLS handshake, Fig: retained heap after connection, Fig: on-wire message size per PUBLISH, annotate_delta(), bar2(), fig_handshake(), fig_heap(), fig_packet() (+8 more)

### Community 4 - "rtt.py"
Cohesion: 0.27
Nodes (6): Fig: end-to-end command RTT, main(), make_client(), percentile(), paho-mqtt 2.x (VERSION2) uz fallback na 1.x., RttMeter

### Community 5 - "mqtt_wrap.c"
Cohesion: 0.23
Nodes (10): esp_event_base_t, meas_heap(), meas_time(), meas_tlscfg(), mqtt_availability_topic(), mqtt_event_handler(), mqtt_start(), mqtt_conn_cb (+2 more)

### Community 6 - "rtos_every_ms"
Cohesion: 0.29
Nodes (5): BaseType_t, rtos_every_ms(), rtos_periodic_fn, TaskHandle_t, UBaseType_t

### Community 7 - "HomeAssistantOrchestration — Dokumentacija implementacije"
Cohesion: 0.08
Nodes (23): 10. Zaključak, 1. Uvod i cilj sistema, 2.1 Topologija sistema, 2.2 Uređaji (ploče), 2.3 Softverski stek, 2. Pregled arhitekture, 3. Build sistem (netrivijalan i bitan za razumevanje), 5.1 MQTT konvencije tema (+15 more)

### Community 8 - "wifi_sta.c"
Cohesion: 0.33
Nodes (3): esp_event_base_t, on_wifi_event(), wifi_sta_start()

### Community 9 - "mqtt-ha-dev.md"
Cohesion: 0.33
Nodes (5): Contracts you must preserve, How you work, Rules, Topic conventions (keep consistent), What you own

### Community 19 - "Embedded agent team — HomeAssistantOrchestration"
Cohesion: 0.33
Nodes (5): Embedded agent team — HomeAssistantOrchestration, How to run the team, Invocation examples (things to say to the lead), Notes, Roster

### Community 20 - "4. Zajednička komponenta `common`"
Cohesion: 0.33
Nodes (6): 4.1 `wifi_sta` — WiFi konektivnost, 4.2 `mqtt_wrap` — MQTT klijent i centralna dispečerska tačka, 4.3 `ha_discovery` — Home Assistant auto-otkrivanje, 4.4 `app_rtos` — periodični FreeRTOS zadaci, 4.5 `ota` — daljinsko ažuriranje firmvera, 4. Zajednička komponenta `common`

### Community 21 - "firmware-dev.md"
Cohesion: 0.40
Nodes (4): Adding a new HA entity (do all three in the board module), How you work, Non-negotiable rules for this codebase, What you own

### Community 22 - "ota-build-dev.md"
Cohesion: 0.40
Nodes (4): Build-system model (important, non-obvious), OTA model, Rules, What you own

### Community 23 - "certs/ — privatni CA za verifikaciju Mosquitto brokera"
Cohesion: 0.40
Nodes (4): `ca.crt`, certs/ — privatni CA za verifikaciju Mosquitto brokera, Zašto ime mora ostati `ca.crt`, Šta se commit-uje

### Community 24 - "build-qa.md"
Cohesion: 0.50
Nodes (3): Environments, Output, Procedure

### Community 25 - "integration-tester.md"
Cohesion: 0.50
Nodes (3): How you work, Scope, What to verify

### Community 26 - "qa-reviewer.md"
Cohesion: 0.50
Nodes (3): General embedded-C review, Output, Project-specific checklist (highest signal)

### Community 27 - "unit-tester.md"
Cohesion: 0.50
Nodes (3): Current state, How you work, What to test (highest value first)

## Knowledge Gaps
- **57 isolated node(s):** `Roster`, `How to run the team`, `Invocation examples (things to say to the lead)`, `Notes`, `Environments` (+52 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `plain-MQTT vs TLS measurement study` connect `plot_results.py` to `ha_publish_config`, `ota.c`, `climate_start`, `rtt.py`?**
  _High betweenness centrality (0.101) - this node is a cross-community bridge._
- **Why does `climate_start()` connect `climate_start` to `ha_publish_config`, `wifi_sta.c`, `mqtt_wrap.c`, `rtos_every_ms`?**
  _High betweenness centrality (0.064) - this node is a cross-community bridge._
- **Why does `ESP2 temperature board` connect `climate_start` to `plot_results.py`?**
  _High betweenness centrality (0.060) - this node is a cross-community bridge._
- **Are the 3 inferred relationships involving `OTA firmware update` (e.g. with `ESP1 lights board` and `ota.h`) actually correct?**
  _`OTA firmware update` has 3 INFERRED edges - model-reasoned connections that need verification._
- **Are the 9 inferred relationships involving `mqtt_publish()` (e.g. with `ha_publish_config()` and `ota_task()`) actually correct?**
  _`mqtt_publish()` has 9 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `mqtt_start()` (e.g. with `ota_start()` and `Last Will availability`) actually correct?**
  _`mqtt_start()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `plain-MQTT vs TLS measurement study` (e.g. with `docs/README.md (architecture diagrams)` and `Fig: connection time / TLS handshake`) actually correct?**
  _`plain-MQTT vs TLS measurement study` has 5 INFERRED edges - model-reasoned connections that need verification._