#!/usr/bin/env python3
"""Sanitize a raw MQTT capture for public sharing.

Replaces cleartext credential bytes (which leak on the plain 1883 channel) with
DEMO values of IDENTICAL length, so the artifact still demonstrates the
cleartext-credential exposure without publishing the real secret. Recomputes
IP/TCP checksums for the modified packets and preserves capture timestamps.
Redactions are passed as OLD=NEW pairs, so the real secrets are never hardcoded.

Writes pcapng (Wireshark-native) when supported, else classic pcap (.cap).

usage: sanitize_pcap.py <src.pcap> <out_base> OLD=NEW [OLD=NEW ...]
  e.g. sanitize_pcap.py mqtt_thesis.pcap mqtt_capture <mqtt-user>=homeuser <mqtt-pass>=N7kQ2pX9
"""
import sys

from scapy.all import rdpcap, wrpcap, IP, TCP, Raw

SRC = sys.argv[1]
OUT_BASE = sys.argv[2]  # without extension

# OLD=NEW redactions from the CLI (same length -> no MQTT length-field edits).
REPL = {}
for _pair in sys.argv[3:]:
    _old, _new = _pair.split("=", 1)
    REPL[_old.encode()] = _new.encode()

pkts = rdpcap(SRC)
modified = 0
for p in pkts:
    if not p.haslayer(Raw):
        continue
    raw = bytes(p[Raw].load)
    new = raw
    for a, b in REPL.items():
        assert len(a) == len(b), "replacement must preserve length"
        new = new.replace(a, b)
    if new != raw:
        p[Raw].load = new
        # force checksum + length recompute on re-serialization
        if p.haslayer(IP):
            del p[IP].chksum
            del p[IP].len
        if p.haslayer(TCP):
            del p[TCP].chksum
        # rebuild so scapy recomputes derived fields, keep timestamp
        t = p.time
        p2 = p.__class__(bytes(p))
        p2.time = t
        p[Raw].load = new  # ensure load stays (p2 used only to recompute)
        modified += 1

# rebuild every packet once so all deleted fields are recomputed, ts preserved
out = []
for p in pkts:
    t = p.time
    p = p.__class__(bytes(p))
    p.time = t
    out.append(p)

wrote = None
try:
    from scapy.all import wrpcapng  # type: ignore
    wrpcapng(OUT_BASE + ".pcapng", out)
    wrote = OUT_BASE + ".pcapng"
except Exception:
    try:
        # some scapy versions: PcapNgWriter
        from scapy.utils import PcapNgWriter  # type: ignore
        with PcapNgWriter(OUT_BASE + ".pcapng") as w:
            for p in out:
                w.write(p)
        wrote = OUT_BASE + ".pcapng"
    except Exception:
        # fallback: classic pcap, but .cap extension (Wireshark opens natively,
        # and dodges the docs/measurement/*.pcap gitignore)
        wrpcap(OUT_BASE + ".cap", out)
        wrote = OUT_BASE + ".cap"

print(f"packets: {len(out)}, modified (cred bytes): {modified}")
print(f"wrote: {wrote}")

# verification: every OLD must be gone, every NEW must be present
blob = open(wrote, "rb").read()
for _old, _new in REPL.items():
    assert _old not in blob, f"LEAK: {_old!r} still present!"
    assert _new in blob, f"replacement {_new!r} missing"
print("verify: redacted tokens ABSENT, replacements present  -> OK")
