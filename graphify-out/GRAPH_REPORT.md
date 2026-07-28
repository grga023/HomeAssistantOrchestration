# Graph Report - HomeAssistantOrchestration  (2026-07-28)

## Corpus Check
- 28 files · ~9,296 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 135 nodes · 208 edges · 20 communities (18 shown, 2 thin omitted)
- Extraction: 78% EXTRACTED · 22% INFERRED · 0% AMBIGUOUS · INFERRED: 46 edges (avg confidence: 0.81)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `4a16693a`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- mqtt_publish
- OTA firmware update
- climate_start
- plot_results.py
- rtt.py
- mqtt_wrap.c
- rtos_every_ms
- HomeAssistantOrchestration — Dokumentacija implementacije
- wifi_sta.c
- 7. Implementacija OTA ažuriranja (glavni doprinos)
- gen-certs.sh
- esp_event_base_t

## God Nodes (most connected - your core abstractions)
1. `OTA firmware update` - 14 edges
2. `HomeAssistantOrchestration — Dokumentacija implementacije` - 13 edges
3. `mqtt_publish()` - 9 edges
4. `plain-MQTT vs TLS measurement study` - 8 edges
5. `ha_publish_config()` - 7 edges
6. `mqtt_start()` - 7 edges
7. `ota_on_connect()` - 7 edges
8. `bar2()` - 7 edges
9. `climate_start()` - 7 edges
10. `esp_https_ota over HTTPS (CA bundle)` - 7 edges

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

## Communities (20 total, 2 thin omitted)

### Community 0 - "mqtt_publish"
Cohesion: 0.22
Nodes (14): ha_publish_config(), ha_set_device(), mqtt_publish(), mqtt_subscribe(), ESP1 lights board, MQTT Discovery, apply_relay(), cmd_topic() (+6 more)

### Community 1 - "OTA firmware update"
Cohesion: 0.25
Nodes (14): mqtt_event_handler(), fetch_manifest(), installed_version(), ota_confirm_running_image(), ota_handle_message(), ota_on_connect(), ota_start(), ota_task() (+6 more)

### Community 2 - "climate_start"
Cohesion: 0.21
Nodes (11): mqtt_is_connected(), esp_err_t, gpio_num_t, ESP2 temperature board, apply_heater(), climate_start(), on_message(), poll_task() (+3 more)

### Community 3 - "plot_results.py"
Cohesion: 0.24
Nodes (16): Fig: connection time / TLS handshake, Fig: retained heap after connection, Fig: on-wire message size per PUBLISH, annotate_delta(), bar2(), fig_handshake(), fig_heap(), fig_packet() (+8 more)

### Community 4 - "rtt.py"
Cohesion: 0.27
Nodes (6): Fig: end-to-end command RTT, main(), make_client(), percentile(), paho-mqtt 2.x (VERSION2) uz fallback na 1.x., RttMeter

### Community 5 - "mqtt_wrap.c"
Cohesion: 0.38
Nodes (5): mqtt_availability_topic(), mqtt_start(), mqtt_conn_cb, mqtt_msg_cb, Last Will availability

### Community 6 - "rtos_every_ms"
Cohesion: 0.29
Nodes (5): BaseType_t, rtos_every_ms(), rtos_periodic_fn, TaskHandle_t, UBaseType_t

### Community 7 - "HomeAssistantOrchestration — Dokumentacija implementacije"
Cohesion: 0.08
Nodes (23): 10. Zaključak, 1. Uvod i cilj sistema, 2.1 Topologija sistema, 2.2 Uređaji (ploče), 2.3 Softverski stek, 2. Pregled arhitekture, 3. Build sistem (netrivijalan i bitan za razumevanje), 4.1 `wifi_sta` — WiFi konektivnost (+15 more)

### Community 8 - "wifi_sta.c"
Cohesion: 0.40
Nodes (3): esp_event_base_t, on_wifi_event(), wifi_sta_start()

### Community 9 - "7. Implementacija OTA ažuriranja (glavni doprinos)"
Cohesion: 0.33
Nodes (6): 7.1 Zahtevi i odluke, 7.2 Centralno povezivanje (bez izmena u kodu ploča), 7.3 Javni API modula `ota`, 7.4 Tok ažuriranja, 7.5 Bezbednosna svojstva, 7. Implementacija OTA ažuriranja (glavni doprinos)

## Knowledge Gaps
- **25 isolated node(s):** `1. Uvod i cilj sistema`, `2.1 Topologija sistema`, `2.2 Uređaji (ploče)`, `2.3 Softverski stek`, `Osnovne komande` (+20 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `plain-MQTT vs TLS measurement study` connect `plot_results.py` to `mqtt_publish`, `OTA firmware update`, `climate_start`, `rtt.py`?**
  _High betweenness centrality (0.170) - this node is a cross-community bridge._
- **Why does `climate_start()` connect `climate_start` to `mqtt_publish`, `wifi_sta.c`, `mqtt_wrap.c`, `rtos_every_ms`?**
  _High betweenness centrality (0.105) - this node is a cross-community bridge._
- **Why does `Fig: end-to-end command RTT` connect `rtt.py` to `plot_results.py`?**
  _High betweenness centrality (0.100) - this node is a cross-community bridge._
- **Are the 3 inferred relationships involving `OTA firmware update` (e.g. with `ESP1 lights board` and `ota.h`) actually correct?**
  _`OTA firmware update` has 3 INFERRED edges - model-reasoned connections that need verification._
- **Are the 8 inferred relationships involving `mqtt_publish()` (e.g. with `ha_publish_config()` and `ota_task()`) actually correct?**
  _`mqtt_publish()` has 8 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `plain-MQTT vs TLS measurement study` (e.g. with `docs/README.md (architecture diagrams)` and `Fig: connection time / TLS handshake`) actually correct?**
  _`plain-MQTT vs TLS measurement study` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `ha_publish_config()` (e.g. with `mqtt_availability_topic()` and `mqtt_publish()`) actually correct?**
  _`ha_publish_config()` has 5 INFERRED edges - model-reasoned connections that need verification._