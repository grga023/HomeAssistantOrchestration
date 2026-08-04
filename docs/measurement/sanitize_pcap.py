#!/usr/bin/env python3
"""Sanitize the real MQTTS-overhead capture for public sharing.

Replaces the real broker credentials (username/password that leak in cleartext
on the plain 1883 channel) with realistic-looking DEMO values of identical
length, so the artifact still demonstrates cleartext-credential exposure without
publishing the user's actual password. Recomputes IP/TCP checksums for the
modified packets and preserves original capture timestamps.

Writes pcapng (Wireshark-native) when supported, else classic pcap (.cap).
"""
import sys

from scapy.all import rdpcap, wrpcap, IP, TCP, Raw

SRC = sys.argv[1]
OUT_BASE = sys.argv[2]  # without extension

# same-length replacements (8 bytes each) -> no MQTT length-field edits needed
REPL = {
    b"mqttpass": b"N7kQ2pX9",   # real broker password -> demo password
    b"mqttuser": b"homeuser",   # real broker username -> demo username
}

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

# verification: no real creds must remain; demo creds must be present
blob = open(wrote, "rb").read()
for secret in (b"mqttpass", b"mqttuser"):
    assert secret not in blob, f"LEAK: {secret!r} still present!"
assert b"N7kQ2pX9" in blob and b"homeuser" in blob, "demo creds missing"
print("verify: real creds ABSENT, demo creds present  -> OK")
