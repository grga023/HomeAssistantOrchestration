# Architecture docs

PlantUML sequence diagrams for the Smart Home demo. Render with the PlantUML
extension, `plantuml docs/*.puml`, or https://www.plantuml.com/plantuml.

| File | Application |
|------|-------------|
| `common_boot.puml`       | Shared WiFi + MQTT + Home Assistant discovery flow (all boards) |
| `esp1_lights.puml`       | ESP1 relay light switches |
| `esp2_temperature.puml`  | ESP2 DHT22 sensor + heater relay |

Both boards share the boot/connectivity flow in `common_boot.puml`; each
board-specific diagram picks up after `on_connect()`.
