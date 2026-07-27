# Graph Report - .  (2026-07-27)

## Corpus Check
- Corpus is ~11,950 words - fits in a single context window. You may not need a graph.

## Summary
- 110 nodes · 179 edges · 20 communities (19 shown, 1 thin omitted)
- Extraction: 78% EXTRACTED · 22% INFERRED · 0% AMBIGUOUS · INFERRED: 39 edges (avg confidence: 0.8)
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

## God Nodes (most connected - your core abstractions)
1. `mqtt_publish()` - 10 edges
2. `transition()` - 9 edges
3. `rtos_every_ms()` - 8 edges
4. `ha_publish_config()` - 7 edges
5. `mqtt_start()` - 7 edges
6. `bar2()` - 7 edges
7. `climate_start()` - 7 edges
8. `publish_state()` - 7 edges
9. `garage_start()` - 7 edges
10. `security_start()` - 7 edges

## Surprising Connections (you probably didn't know these)
- `garage_start()` --calls--> `rtos_every_ms()`  [INFERRED]
  src/esp3_garage/src/garage.c → components/common/src/app_rtos.c
- `lights_start()` --calls--> `ha_set_device()`  [INFERRED]
  src/esp1_lights/src/lights.c → components/common/src/ha_discovery.c
- `garage_start()` --calls--> `ha_set_device()`  [INFERRED]
  src/esp3_garage/src/garage.c → components/common/src/ha_discovery.c
- `on_connect()` --calls--> `ha_publish_config()`  [INFERRED]
  src/esp1_lights/src/lights.c → components/common/src/ha_discovery.c
- `lights_start()` --calls--> `mqtt_start()`  [INFERRED]
  src/esp1_lights/src/lights.c → components/common/src/mqtt_wrap.c

## Import Cycles
- None detected.

## Communities (20 total, 1 thin omitted)

### Community 0 - "Community 0"
Cohesion: 0.11
Nodes (15): BaseType_t, rtos_every_ms(), ha_set_device(), mqtt_start(), mqtt_conn_cb, mqtt_msg_cb, rtos_periodic_fn, apply_heater() (+7 more)

### Community 1 - "Community 1"
Cohesion: 0.30
Nodes (12): door_state_t, garage_start(), now_ms(), on_message(), publish_state(), pulse_relay(), reed_closed(), state_str() (+4 more)

### Community 2 - "Community 2"
Cohesion: 0.38
Nodes (11): alarm_t, alarm_str(), arm(), door_open(), motion(), now_ms(), on_message(), publish_alarm() (+3 more)

### Community 3 - "Community 3"
Cohesion: 0.26
Nodes (11): ha_publish_config(), esp_event_base_t, mqtt_availability_topic(), mqtt_event_handler(), mqtt_is_connected(), mqtt_publish(), mqtt_subscribe(), on_connect() (+3 more)

### Community 4 - "Community 4"
Cohesion: 0.40
Nodes (10): annotate_delta(), bar2(), fig_handshake(), fig_heap(), fig_packet(), fig_rtt(), load_results(), main() (+2 more)

### Community 5 - "Community 5"
Cohesion: 0.31
Nodes (5): main(), make_client(), percentile(), paho-mqtt 2.x (VERSION2) uz fallback na 1.x., RttMeter

### Community 6 - "Community 6"
Cohesion: 0.39
Nodes (7): apply_relay(), cmd_topic(), lights_start(), on_connect(), on_message(), state_topic(), app_main()

### Community 7 - "Community 7"
Cohesion: 0.40
Nodes (3): esp_event_base_t, on_wifi_event(), wifi_sta_start()

### Community 8 - "Community 8"
Cohesion: 0.60
Nodes (4): esp_err_t, gpio_num_t, dht22_read(), wait_level()

## Knowledge Gaps
- **1 isolated node(s):** `gen-certs.sh script`
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `mqtt_publish()` connect `Community 3` to `Community 0`, `Community 1`, `Community 2`, `Community 6`?**
  _High betweenness centrality (0.137) - this node is a cross-community bridge._
- **Why does `security_start()` connect `Community 0` to `Community 2`, `Community 7`?**
  _High betweenness centrality (0.094) - this node is a cross-community bridge._
- **Why does `garage_start()` connect `Community 1` to `Community 0`, `Community 7`?**
  _High betweenness centrality (0.092) - this node is a cross-community bridge._
- **Are the 9 inferred relationships involving `mqtt_publish()` (e.g. with `ha_publish_config()` and `apply_relay()`) actually correct?**
  _`mqtt_publish()` has 9 INFERRED edges - model-reasoned connections that need verification._
- **Are the 3 inferred relationships involving `rtos_every_ms()` (e.g. with `climate_start()` and `garage_start()`) actually correct?**
  _`rtos_every_ms()` has 3 INFERRED edges - model-reasoned connections that need verification._
- **Are the 6 inferred relationships involving `ha_publish_config()` (e.g. with `mqtt_availability_topic()` and `mqtt_publish()`) actually correct?**
  _`ha_publish_config()` has 6 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `mqtt_start()` (e.g. with `lights_start()` and `climate_start()`) actually correct?**
  _`mqtt_start()` has 4 INFERRED edges - model-reasoned connections that need verification._