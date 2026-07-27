# Graph Report - .  (2026-07-27)

## Corpus Check
- Corpus is ~7,511 words - fits in a single context window. You may not need a graph.

## Summary
- 107 nodes · 181 edges · 19 communities (17 shown, 2 thin omitted)
- Extraction: 75% EXTRACTED · 25% INFERRED · 0% AMBIGUOUS · INFERRED: 46 edges (avg confidence: 0.81)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Community 0
- Community 1
- Community 2
- Community 3
- Community 4
- Community 5
- Community 6
- Community 7
- Community 8
- Community 9
- Community 10

## God Nodes (most connected - your core abstractions)
1. `OTA firmware update` - 14 edges
2. `mqtt_publish()` - 9 edges
3. `plain-MQTT vs TLS measurement study` - 8 edges
4. `ha_publish_config()` - 7 edges
5. `mqtt_start()` - 7 edges
6. `ota_on_connect()` - 7 edges
7. `bar2()` - 7 edges
8. `climate_start()` - 7 edges
9. `esp_https_ota over HTTPS (CA bundle)` - 7 edges
10. `rtos_every_ms()` - 6 edges

## Surprising Connections (you probably didn't know these)
- `esp_https_ota over HTTPS (CA bundle)` --references--> `start_ota()`  [INFERRED]
  README.md → components/common/src/ota.c
- `plain-MQTT vs TLS measurement study` --references--> `Fig: end-to-end command RTT`  [INFERRED]
  README.md → docs/measurement/fig-rtt.png
- `Last Will availability` --references--> `mqtt_start()`  [INFERRED]
  README.md → components/common/src/mqtt_wrap.c
- `OTA firmware update` --references--> `fetch_manifest()`  [INFERRED]
  README.md → components/common/src/ota.c
- `OTA firmware update` --references--> `ota_start()`  [EXTRACTED]
  README.md → components/common/src/ota.c

## Import Cycles
- None detected.

## Communities (19 total, 2 thin omitted)

### Community 0 - "Community 0"
Cohesion: 0.22
Nodes (14): ha_publish_config(), ha_set_device(), mqtt_publish(), mqtt_subscribe(), ESP1 lights board, MQTT Discovery, apply_relay(), cmd_topic() (+6 more)

### Community 1 - "Community 1"
Cohesion: 0.27
Nodes (13): esp_event_base_t, mqtt_event_handler(), fetch_manifest(), installed_version(), ota_confirm_running_image(), ota_handle_message(), ota_on_connect(), ota_task() (+5 more)

### Community 2 - "Community 2"
Cohesion: 0.21
Nodes (11): mqtt_is_connected(), esp_err_t, gpio_num_t, ESP2 temperature board, apply_heater(), climate_start(), on_message(), poll_task() (+3 more)

### Community 3 - "Community 3"
Cohesion: 0.40
Nodes (10): annotate_delta(), bar2(), fig_handshake(), fig_heap(), fig_packet(), fig_rtt(), load_results(), main() (+2 more)

### Community 4 - "Community 4"
Cohesion: 0.27
Nodes (6): Fig: end-to-end command RTT, main(), make_client(), percentile(), paho-mqtt 2.x (VERSION2) uz fallback na 1.x., RttMeter

### Community 5 - "Community 5"
Cohesion: 0.32
Nodes (6): mqtt_availability_topic(), mqtt_start(), ota_start(), mqtt_conn_cb, mqtt_msg_cb, Last Will availability

### Community 6 - "Community 6"
Cohesion: 0.29
Nodes (5): BaseType_t, rtos_every_ms(), rtos_periodic_fn, TaskHandle_t, UBaseType_t

### Community 7 - "Community 7"
Cohesion: 0.47
Nodes (6): Fig: connection time / TLS handshake, Fig: retained heap after connection, Fig: on-wire message size per PUBLISH, docs/README.md (architecture diagrams), esp_https_ota over HTTPS (CA bundle), plain-MQTT vs TLS measurement study

### Community 8 - "Community 8"
Cohesion: 0.40
Nodes (3): esp_event_base_t, on_wifi_event(), wifi_sta_start()

## Knowledge Gaps
- **2 isolated node(s):** `gen-certs.sh script`, `docs/README.md (architecture diagrams)`
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `plain-MQTT vs TLS measurement study` connect `Community 7` to `Community 0`, `Community 1`, `Community 2`, `Community 4`?**
  _High betweenness centrality (0.272) - this node is a cross-community bridge._
- **Why does `climate_start()` connect `Community 2` to `Community 0`, `Community 8`, `Community 5`, `Community 6`?**
  _High betweenness centrality (0.167) - this node is a cross-community bridge._
- **Why does `Fig: end-to-end command RTT` connect `Community 4` to `Community 3`, `Community 7`?**
  _High betweenness centrality (0.160) - this node is a cross-community bridge._
- **Are the 3 inferred relationships involving `OTA firmware update` (e.g. with `ESP1 lights board` and `ota.h`) actually correct?**
  _`OTA firmware update` has 3 INFERRED edges - model-reasoned connections that need verification._
- **Are the 8 inferred relationships involving `mqtt_publish()` (e.g. with `ha_publish_config()` and `ota_task()`) actually correct?**
  _`mqtt_publish()` has 8 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `plain-MQTT vs TLS measurement study` (e.g. with `docs/README.md (architecture diagrams)` and `Fig: connection time / TLS handshake`) actually correct?**
  _`plain-MQTT vs TLS measurement study` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `ha_publish_config()` (e.g. with `mqtt_availability_topic()` and `mqtt_publish()`) actually correct?**
  _`ha_publish_config()` has 5 INFERRED edges - model-reasoned connections that need verification._