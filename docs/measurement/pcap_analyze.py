#!/usr/bin/env python3
"""pcap_analyze.py - METRIC 3 analysis for the MQTTS-overhead study.

Reads a pcap captured on the broker host (tcp 1883 + 8883) and reports:
  * per-port and per-device byte/packet totals (plain vs TLS conversation cost)
  * TLS handshake size on 8883 (one-time)
  * encryption proof: readable MQTT tokens on 1883 vs none on 8883
  * matched per-PUBLISH wire size: identical topic+payload sent plain vs TLS
"""
import argparse, collections, re
from scapy.all import rdpcap, IP, TCP, Raw

TOKENS = [b"home/", b"homeassistant/", b"online", b"offline", b'{"t1"',
          b"light", b"temperature", b"test/meas", b"ON", b"OFF"]


def tcp_payload(pkt):
    return bytes(pkt[Raw].load) if pkt.haslayer(Raw) else b""


def tls_records(payload):
    """Yield (content_type, length) for TLS records in a TCP payload."""
    i = 0
    while i + 5 <= len(payload):
        ctype = payload[i]
        ver = (payload[i+1] << 8) | payload[i+2]
        ln = (payload[i+3] << 8) | payload[i+4]
        if ctype not in (20, 21, 22, 23) or ver not in (0x0301, 0x0302, 0x0303, 0x0304):
            break
        yield ctype, ln
        i += 5 + ln


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pcap")
    ap.add_argument("--esp1", default="192.168.0.11")
    ap.add_argument("--esp2", default="192.168.0.12")
    ap.add_argument("--test-topic", default="test/meas")
    ap.add_argument("--test-len", type=int, default=45,
                    help="expected plain MQTT PUBLISH byte length of the matched test msg")
    args = ap.parse_args()

    pkts = rdpcap(args.pcap)
    port_bytes = collections.Counter()
    port_pkts = collections.Counter()
    dev_bytes = collections.Counter()
    hs_bytes_8883 = 0
    hs_records_8883 = 0
    appdata_8883 = collections.Counter()   # length -> count
    readable_1883 = []
    plaintext_on_8883 = 0
    plain_pub_sizes = []
    test_re = args.test_topic.encode()

    for p in pkts:
        if not (p.haslayer(TCP) and p.haslayer(IP)):
            continue
        sport, dport = p[TCP].sport, p[TCP].dport
        port = 1883 if 1883 in (sport, dport) else (8883 if 8883 in (sport, dport) else None)
        if port is None:
            continue
        pl = tcp_payload(p)
        ln = len(pl)
        port_pkts[port] += 1
        port_bytes[port] += ln
        src, dst = p[IP].src, p[IP].dst
        for dev, name in ((args.esp1, "esp1_plain"), (args.esp2, "esp2_tls")):
            if src == dev or dst == dev:
                dev_bytes[name] += ln
        if ln == 0:
            continue
        if port == 8883:
            for ctype, rlen in tls_records(pl):
                if ctype == 22:           # handshake
                    hs_bytes_8883 += rlen + 5
                    hs_records_8883 += 1
                elif ctype == 23:         # application data
                    appdata_8883[rlen + 5] += 1
            # plaintext MQTT leaking on TLS port? (should never happen)
            if any(t in pl for t in (b"home/", b"homeassistant/", b'{"t1"', test_re)):
                plaintext_on_8883 += 1
        else:  # 1883 cleartext
            hits = [t for t in TOKENS if t in pl]
            if hits and len(readable_1883) < 6:
                snippet = re.sub(rb"[^\x20-\x7e]", b".", pl[:80]).decode()
                readable_1883.append((hits, snippet))
            # matched test publish: MQTT PUBLISH (first byte 0x30..0x3f) containing test topic
            if test_re in pl and pl and (pl[0] & 0xF0) == 0x30:
                plain_pub_sizes.append(ln)

    print("=" * 62)
    print("METRIC 3 - NETWORK OVERHEAD (pcap: %s, %d packets)" % (args.pcap, len(pkts)))
    print("=" * 62)
    print("\n[conversation totals]")
    for port in (1883, 8883):
        print(f"  port {port}: {port_pkts[port]:5d} pkts, {port_bytes[port]:7d} payload bytes")
    for name in ("esp1_plain", "esp2_tls"):
        print(f"  {name:10s}: {dev_bytes[name]:7d} payload bytes")

    print("\n[TLS handshake on 8883 - one-time cost]")
    print(f"  handshake records: {hs_records_8883}, bytes (incl 5B hdrs): {hs_bytes_8883}")

    print("\n[encryption proof]")
    print(f"  readable MQTT tokens found on 1883 (cleartext): {len(readable_1883)} sample(s)")
    for hits, snip in readable_1883:
        print(f"     {[h.decode() for h in hits]}: {snip}")
    print(f"  plaintext MQTT leaking on 8883 (should be 0): {plaintext_on_8883}")
    print(f"  8883 carries only opaque TLS Application Data records "
          f"({sum(appdata_8883.values())} records)")

    print("\n[matched per-PUBLISH wire size: identical topic+payload]")
    if plain_pub_sizes:
        pmin = min(plain_pub_sizes)
        print(f"  plain (1883) MQTT PUBLISH TCP-payload: {sorted(set(plain_pub_sizes))} B "
              f"(n={len(plain_pub_sizes)})")
    else:
        pmin = args.test_len
        print(f"  plain (1883): not found; using expected {pmin} B")
    # TLS app-data record whose size == plain + [21..29] typical GCM overhead
    cand = {sz: c for sz, c in appdata_8883.items() if pmin + 15 <= sz <= pmin + 40}
    print(f"  TLS (8883) app-data record sizes near matched publish: {dict(sorted(cand.items()))}")
    if cand:
        tls_sz = max(cand, key=cand.get)
        print(f"  => plain {pmin} B  vs  TLS {tls_sz} B  =>  +{tls_sz - pmin} B TLS overhead/PUBLISH")
    print("\n  all 8883 app-data record sizes (size:count):",
          dict(sorted(appdata_8883.items())))


if __name__ == "__main__":
    main()
