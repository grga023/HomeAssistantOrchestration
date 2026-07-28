# certs/ — privatni CA za verifikaciju Mosquitto brokera

Ovaj direktorijum drži *javni* sertifikat privatnog CA (`ca.crt`) koji esp2
ugrađuje u firmware da bi verifikovao Mosquitto broker preko MQTTS-a (8883).
Detalji: `docs/measurement/03-broker-pki-tls.adoc`.

## `ca.crt`

- Trenutno je **DEV PLACEHOLDER** — postoji samo da CMake `configure` +
  `target_add_binary_data(... TEXT)` prođu za `[env:esp2_temperature]`.
  TLS validnost je runtime/hardware stvar, ne build stvar.
- **Pravi** `ca.crt` generiši na Raspberry Pi-ju sa
  `docs/measurement/broker/gen-certs.sh` i njime zameni placeholder **pre bilo
  kog TLS runa**.

## Šta se commit-uje

- Commit-uj **SAMO** `ca.crt` (javni je — bezbedno).
- **NIKAD** ne commit-uj `ca.key` ni `server.key` (privatni ključevi ostaju na RPi).

## Zašto ime mora ostati `ca.crt`

Embed simbol se izvodi iz osnovnog imena fajla:
`certs/ca.crt` → `_binary_ca_crt_start` / `_binary_ca_crt_end`.
`mqtt_wrap.c` referencira baš `_binary_ca_crt_start`, pa preimenovanje fajla
obara linkovanje.
