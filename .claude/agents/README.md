# Embedded agent team — HomeAssistantOrchestration

Persistent, project-specific specialist agents. These `.md` files are the **team**:
the definitions live in the repo forever, and any Claude Code session can spawn the
specialists on demand (one or several instances each). Running agents are
session-scoped; the *roster* is permanent.

## Roster

| Agent | Role | Owns |
|-------|------|------|
| `firmware-dev` | Board-module developer | `src/<board>/` — lights.c, climate.c, dht22.c, main.c, GPIO, RTOS ticks, HA-entity wiring |
| `mqtt-ha-dev` | MQTT + HA discovery dev | `mqtt_wrap`, `ha_discovery`, `wifi_sta`, TLS, topic conventions |
| `ota-build-dev` | OTA + build system dev | `ota.c`, `src/CMakeLists.txt` glob, `platformio.ini`, partitions, sdkconfig, releases |
| `unit-tester` | Unit tests (Unity/PlatformIO) | `test/` — pure-logic tests (topic match, active_low, discovery, manifest parse) |
| `integration-tester` | E2E / HW-in-loop | broker↔firmware↔HA, LWT, command round-trips, OTA flow, TLS |
| `qa-reviewer` | Read-only code review | correctness audit vs project gotchas; reports, never edits |
| `build-qa` | Build + warning gate | compiles the affected env(s), reports real errors/warnings |

The lead (main session) is the **tech-lead/orchestrator** — it plans, splits work
into shared tasks, spawns specialists, and integrates their results.

## How to run the team

1. **Lead breaks the work into a shared task list** (`TaskCreate`), with
   dependencies (`addBlockedBy`) — e.g. dev tasks block their test/QA tasks.
2. **Lead spawns the needed specialists** with the `Agent` tool (`subagent_type` =
   agent name). Independent agents go in one message so they run in parallel. Spawn
   **multiple instances** of the same role when work fans out (e.g. two `firmware-dev`
   on two boards at once).
3. **Specialists coordinate** via `SendMessage` (to each other by name, or to `main`)
   and pull/close tasks via `TaskList`/`TaskUpdate`.
4. **Standard pipeline** for a feature:
   `firmware-dev` / `mqtt-ha-dev` / `ota-build-dev` (implement) →
   `build-qa` (compiles clean?) → `unit-tester` + `integration-tester` (behavior) →
   `qa-reviewer` (final correctness gate) → lead integrates & reports.

## Invocation examples (things to say to the lead)

- "Add a second relay channel to esp1_lights, tested and reviewed."
  → lead: `firmware-dev` implements → `build-qa` builds `esp1_lights` →
    `unit-tester` covers topic match + active_low → `qa-reviewer` audits.
- "Harden the OTA manifest parsing."
  → `ota-build-dev` + `unit-tester` (parse edge cases) + `qa-reviewer`.
- "Full pre-release check."
  → `build-qa` (both envs) + `integration-tester` (E2E) + `qa-reviewer` in parallel.

## Notes
- `build-qa` needs `components/common/include/secrets.h` to exist (from
  `secrets.example.h`) before any build. It reports rather than invents credentials.
- QA is read-only by design; devs apply fixes it reports.
- Model/effort per agent inherit the session unless a definition overrides them.
