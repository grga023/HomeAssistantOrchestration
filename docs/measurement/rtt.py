#!/usr/bin/env python3
"""
rtt.py — end-to-end command RTT (4. merenje) za master rad "MQTTS overhead".

Meri koliko traje da komanda objavljena na `.../set` proizvede echo na `.../state`,
sve na JEDNOM satu (time.perf_counter). Pokreni protiv esp1 (plain) i esp2 (TLS)
i uporedi; oba p95 treba da budu ispod 200 ms (hipoteza rada).

Ispravke ugrađene (iz analize):
  * state topici su RETAINED -> na subscribe stigne poslednja poruka (lažni ~0 ms).
    Skripta prvo DRENIRA (ignoriše) sve što stigne u prvih ~0.5 s.
  * payload-i se ponavljaju -> skripta ALTERNIRA ON/OFF svake iteracije, pa svaka
    komanda proizvede razlučiv, sveže-tajmovan echo (matchuje se na očekivanu vrednost).
  * QoS 0 u oba smera; jedan perf_counter sat.

Zahteva: pip install "paho-mqtt>=2.0"

Primeri:
  # esp1 (plain, port 1883)
  python3 rtt.py --host 192.168.1.10 --port 1883 -u mqttuser -P mqttpass \
      --set home/lights/light1/set --state home/lights/light1/state -n 100

  # esp2 (puna TLS deonica, port 8883)
  python3 rtt.py --host 192.168.1.10 --port 8883 -u mqttuser -P mqttpass \
      --cafile broker/ca.crt \
      --set home/temperature/heater/set --state home/temperature/heater/state -n 100
"""
import argparse
import ssl
import sys
import threading
import time

import paho.mqtt.client as mqtt


def make_client(client_id):
    """paho-mqtt 2.x (VERSION2) uz fallback na 1.x."""
    try:
        from paho.mqtt.enums import CallbackAPIVersion
        return mqtt.Client(CallbackAPIVersion.VERSION2, client_id=client_id), 2
    except (ImportError, AttributeError, TypeError):
        return mqtt.Client(client_id=client_id), 1


class RttMeter:
    def __init__(self, state_topic):
        self.state_topic = state_topic
        self.expected = None            # None dok drenira; inače očekivana vrednost
        self.recv_time = None
        self.event = threading.Event()
        self.connected = threading.Event()

    # --- callbacks (rade i za V1 i za V2 potpise) ---
    def on_connect(self, client, userdata, flags, reason_code, properties=None):
        rc = getattr(reason_code, "value", reason_code)   # paho 2.x ReasonCode -> int
        if rc == 0:
            client.subscribe(self.state_topic, qos=0)
            self.connected.set()
        else:
            print(f"[!] Konekcija odbijena, rc={reason_code}", file=sys.stderr)

    def on_message(self, client, userdata, msg):
        payload = msg.payload.decode("utf-8", "replace").strip()
        if self.expected is not None and payload == self.expected:
            self.recv_time = time.perf_counter()
            self.event.set()


def percentile(values, pct):
    if not values:
        return float("nan")
    s = sorted(values)
    idx = min(len(s) - 1, int(round((pct / 100.0) * (len(s) - 1))))
    return s[idx]


def main():
    ap = argparse.ArgumentParser(description="MQTT command RTT (set->state).")
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--set", dest="set_topic", required=True)
    ap.add_argument("--state", dest="state_topic", required=True)
    ap.add_argument("-u", "--user", default=None)
    ap.add_argument("-P", "--password", default=None)
    ap.add_argument("-n", "--count", type=int, default=100)
    ap.add_argument("--interval", type=float, default=0.25,
                    help="pauza između komandi (s), default 0.25")
    ap.add_argument("--timeout", type=float, default=5.0,
                    help="max čekanje echa po komandi (s)")
    ap.add_argument("--cafile", default=None,
                    help="CA cert za TLS (port 8883). Ako se zada, uključuje TLS.")
    ap.add_argument("--insecure", action="store_true",
                    help="TLS bez provere imena hosta (skip CN/SAN).")
    args = ap.parse_args()

    meter = RttMeter(args.state_topic)
    client, api = make_client("rtt-meter")
    client.on_connect = meter.on_connect
    client.on_message = meter.on_message
    if args.user is not None:
        client.username_pw_set(args.user, args.password)
    if args.cafile:
        client.tls_set(ca_certs=args.cafile, tls_version=ssl.PROTOCOL_TLS_CLIENT)
        if args.insecure:
            client.tls_insecure_set(True)

    print(f">> Povezivanje na {args.host}:{args.port} "
          f"({'TLS' if args.cafile else 'plain'}, paho API v{api})")
    client.connect(args.host, args.port, keepalive=30)
    client.loop_start()

    if not meter.connected.wait(timeout=10):
        print("[!] Nema konekcije za 10 s — proveri host/port/kredencijale/TLS.",
              file=sys.stderr)
        client.loop_stop()
        sys.exit(1)

    # Dreniraj retained/zaostale poruke ~0.5 s (expected je None -> sve se ignoriše).
    time.sleep(0.5)

    results = []
    drops = 0
    for i in range(args.count):
        value = "ON" if (i % 2 == 0) else "OFF"   # alternacija -> razlučiv echo
        meter.expected = value
        meter.event.clear()
        t0 = time.perf_counter()
        client.publish(args.set_topic, value, qos=0)
        if meter.event.wait(timeout=args.timeout):
            rtt_ms = (meter.recv_time - t0) * 1000.0
            results.append(rtt_ms)
        else:
            drops += 1
            print(f"[!] iter {i}: timeout (nema echa za '{value}')", file=sys.stderr)
        time.sleep(args.interval)

    client.loop_stop()
    client.disconnect()

    print("\n===== REZULTAT =====")
    print(f"topic set:   {args.set_topic}")
    print(f"topic state: {args.state_topic}")
    print(f"kanal:       {'TLS 8883' if args.cafile else 'plain 1883'}")
    print(f"n (poslato): {args.count}   uspešno: {len(results)}   drops: {drops}")
    if results:
        mean = sum(results) / len(results)
        print(f"RTT mean:    {mean:7.2f} ms   (jednosmerno ~ {mean/2:5.2f} ms)")
        print(f"RTT median:  {percentile(results, 50):7.2f} ms")
        print(f"RTT p95:     {percentile(results, 95):7.2f} ms")
        print(f"RTT min/max: {min(results):7.2f} / {max(results):7.2f} ms")
        verdict = "OK (< 200 ms)" if percentile(results, 95) < 200 else "PREKO 200 ms — istraži!"
        print(f"hipoteza:    {verdict}")


if __name__ == "__main__":
    main()
