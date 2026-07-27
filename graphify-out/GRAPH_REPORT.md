# Graph Report - .  (2026-07-27)

## Corpus Check
- Corpus is ~11,950 words - fits in a single context window. You may not need a graph.

## Summary
- 124 nodes · 208 edges · 20 communities (18 shown, 2 thin omitted)
- Extraction: 77% EXTRACTED · 23% INFERRED · 0% AMBIGUOUS · INFERRED: 48 edges (avg confidence: 0.81)
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
- Community 14

## God Nodes (most connected - your core abstractions)
1. `mqtt_publish()` - 10 edges
2. `transition()` - 10 edges
3. `rtos_every_ms()` - 8 edges
4. `ha_publish_config()` - 8 edges
5. `mqtt_start()` - 8 edges
6. `climate_start()` - 8 edges
7. `garage_start()` - 8 edges
8. `security_start()` - 8 edges
9. `bar2()` - 7 edges
10. `lights_start()` - 7 edges

## Surprising Connections (you probably didn't know these)
- `MQTT Discovery (HA auto-entities)` --references--> `ha_publish_config()`  [INFERRED]
  README.md → components/common/src/ha_discovery.c
- `MQTT Availability / Last Will` --references--> `mqtt_availability_topic()`  [INFERRED]
  CLAUDE.md → components/common/src/mqtt_wrap.c
- `Non-null-terminated MQTT topic/data gotcha` --references--> `on_message()`  [INFERRED]
  CLAUDE.md → src/esp1_lights/src/lights.c
- `Non-null-terminated MQTT topic/data gotcha` --references--> `on_message()`  [INFERRED]
  CLAUDE.md → src/esp2_temperature/src/climate.c
- `Non-null-terminated MQTT topic/data gotcha` --references--> `on_message()`  [INFERRED]
  CLAUDE.md → src/esp3_garage/src/garage.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **** — docs_measurement_fig_handshake, docs_measurement_fig_heap, docs_measurement_fig_packet, docs_measurement_fig_rtt, measurement_mqtts_tls_overhead [INFERRED 0.85]

## Communities (20 total, 2 thin omitted)

### Community 0 - "Community 0"
Cohesion: 0.10
Nodes (22): CLAUDE.md (Repo Guidance), MQTT Availability / Last Will, Board Selection Build Mechanism, Non-null-terminated MQTT topic/data gotcha, Per-board *_start() Pattern, ha_set_device(), mqtt_start(), esp_event_base_t (+14 more)

### Community 1 - "Community 1"
Cohesion: 0.21
Nodes (15): ha_publish_config(), esp_event_base_t, mqtt_availability_topic(), mqtt_event_handler(), mqtt_is_connected(), mqtt_publish(), mqtt_subscribe(), apply_relay() (+7 more)

### Community 2 - "Community 2"
Cohesion: 0.25
Nodes (14): Figure: Connection / TLS Handshake Time, Figure: Retained Heap After Connection, Figure: On-wire Message Size per PUBLISH, Figure: End-to-end Command RTT, annotate_delta(), bar2(), fig_handshake(), fig_heap() (+6 more)

### Community 3 - "Community 3"
Cohesion: 0.18
Nodes (9): BaseType_t, rtos_every_ms(), rtos_periodic_fn, apply_heater(), climate_start(), on_message(), app_main(), TaskHandle_t (+1 more)

### Community 4 - "Community 4"
Cohesion: 0.39
Nodes (11): alarm_t, Security Alarm State Machine (alarm_control_panel), alarm_str(), arm(), door_open(), motion(), now_ms(), on_message() (+3 more)

### Community 5 - "Community 5"
Cohesion: 0.38
Nodes (11): door_state_t, now_ms(), on_connect(), on_message(), publish_state(), pulse_relay(), reed_closed(), state_str() (+3 more)

### Community 6 - "Community 6"
Cohesion: 0.31
Nodes (5): main(), make_client(), percentile(), paho-mqtt 2.x (VERSION2) uz fallback na 1.x., RttMeter

### Community 7 - "Community 7"
Cohesion: 0.60
Nodes (4): esp_err_t, gpio_num_t, dht22_read(), wait_level()

## Knowledge Gaps
- **6 isolated node(s):** `gen-certs.sh script`, `Figure: Connection / TLS Handshake Time`, `Figure: Retained Heap After Connection`, `Figure: On-wire Message Size per PUBLISH`, `Figure: End-to-end Command RTT` (+1 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `mqtt_publish()` connect `Community 1` to `Community 3`, `Community 4`, `Community 5`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._
- **Why does `security_start()` connect `Community 0` to `Community 3`, `Community 4`?**
  _High betweenness centrality (0.087) - this node is a cross-community bridge._
- **Why does `garage_start()` connect `Community 0` to `Community 3`, `Community 5`?**
  _High betweenness centrality (0.084) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `mqtt_publish()` (e.g. with `ha_publish_config()` and `apply_relay()`) actually correct?**
  _`mqtt_publish()` has 9 INFERRED edges - model-reasoned connections that need verification._
- **Are the 3 inferred relationships involving `rtos_every_ms()` (e.g. with `climate_start()` and `garage_start()`) actually correct?**
  _`rtos_every_ms()` has 3 INFERRED edges - model-reasoned connections that need verification._
- **Are the 7 inferred relationships involving `ha_publish_config()` (e.g. with `mqtt_availability_topic()` and `mqtt_publish()`) actually correct?**
  _`ha_publish_config()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `mqtt_start()` (e.g. with `lights_start()` and `climate_start()`) actually correct?**
  _`mqtt_start()` has 4 INFERRED edges - model-reasoned connections that need verification._