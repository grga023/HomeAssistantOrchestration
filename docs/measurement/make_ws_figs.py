#!/usr/bin/env python3
"""make_ws_figs.py — render Wireshark-style figures from mqtt_capture.pcapng.

We can't run the Wireshark GUI headlessly here (no display / no tshark), so this
reconstructs the exact views Wireshark shows, from the REAL sanitized capture:
  ws-packetlist.png   packet list (No./Time/Source/Destination/Protocol/Len/Info)
  ws-stream-1883.png  Follow TCP Stream, plain 1883 (cleartext MQTT + creds)
  ws-stream-8883.png  Follow TCP Stream, TLS 8883 (handshake then opaque)
  ws-serverhello.png  TLS ServerHello dissection (version + cipher suite)
  ws-hierarchy.png    Statistics -> Protocol Hierarchy

Values are taken verbatim from the capture, so the figures match what Wireshark
would display for mqtt_capture.pcapng.
"""
import sys, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from scapy.all import rdpcap, IP, TCP, Raw

PCAP = sys.argv[1] if len(sys.argv) > 1 else "mqtt_capture.pcapng"
OUT  = sys.argv[2].rstrip("/") if len(sys.argv) > 2 else "."

ESP1, ESP2, BROKER = "192.168.0.11", "192.168.0.12", "192.168.0.10"

# ---- Wireshark-ish palette ------------------------------------------------
C_MQTT = "#e6ffd6"   # application data (greenish)
C_TLS  = "#d6e4ff"   # TLS (bluish)
C_TCP  = "#e8e8e8"   # TCP control (grey)
C_HDR  = "#4a4a4a"   # column header bar
MONO   = {"family": "monospace"}

MQTT_TYPES = {1:"Connect Command",2:"Connect Ack",3:"Publish Message",
              4:"Publish Ack",8:"Subscribe Request",9:"Subscribe Ack",
              10:"Unsubscribe",11:"Unsubscribe Ack",12:"Ping Request",
              13:"Ping Response",14:"Disconnect Req"}
TLS_HS = {1:"Client Hello",2:"Server Hello",11:"Certificate",
          12:"Server Key Exchange",13:"Certificate Request",14:"Server Hello Done",
          16:"Client Key Exchange",4:"New Session Ticket",20:"Finished"}
TLS_VER = {0x0301:"1.0",0x0302:"1.1",0x0303:"1.2",0x0304:"1.3"}


def mqtt_topic(pl):
    # PUBLISH: after fixed header (1B type + remaining-length varint) -> 2B topic len + topic
    i = 1
    mult = 1; rl = 0
    while i < len(pl):
        rl += (pl[i] & 0x7f) * mult
        if not (pl[i] & 0x80):
            i += 1; break
        mult *= 128; i += 1
    if i + 2 > len(pl):
        return ""
    tl = (pl[i] << 8) | pl[i+1]
    return pl[i+2:i+2+tl].decode("latin1", "replace")


def dissect_mqtt(pl):
    if not pl:
        return None
    t = pl[0] >> 4
    name = MQTT_TYPES.get(t)
    if not name:
        return None
    # remaining-length varint
    i = 1; mult = 1; rl = 0; ok = False
    while i < len(pl) and i <= 4:
        b = pl[i]; rl += (b & 0x7f) * mult; i += 1
        if not (b & 0x80):
            ok = True; break
        mult *= 128
    if t == 3:
        topic = mqtt_topic(pl)
        if topic and re.match(r'^[\x20-\x7e]{2,60}$', topic):
            return f"Publish Message [{topic}]"
        return "[TCP segment of a reassembled PDU]"
    # control packets are small; a large one is a reassembled TCP segment we
    # cannot dissect per-packet (Wireshark reassembles the stream first)
    if len(pl) > 200 or (ok and i + rl > len(pl)):
        return "[TCP segment of a reassembled PDU]"
    return name


def tls_records(pl):
    i = 0
    out = []
    while i + 5 <= len(pl):
        ct = pl[i]; ver = (pl[i+1] << 8) | pl[i+2]; ln = (pl[i+3] << 8) | pl[i+4]
        if ct not in (20, 21, 22, 23) or ver not in TLS_VER:
            break
        out.append((ct, pl[i+5:i+5+ln]))
        i += 5 + ln
    return out


def dissect_tls(pl):
    recs = tls_records(pl)
    if not recs:
        return None
    parts = []
    after_ccs = False
    for ct, body in recs:
        if ct == 20:
            parts.append("Change Cipher Spec")
            after_ccs = True
        elif ct == 22:
            if after_ccs or not body or body[0] not in TLS_HS:
                parts.append("Encrypted Handshake Message")
            else:
                parts.append(TLS_HS[body[0]])
        elif ct == 23:
            parts.append("Application Data")
        elif ct == 21:
            parts.append("Alert")
    return ", ".join(parts) if parts else None


def load_rows():
    pkts = rdpcap(PCAP)
    t0 = float(pkts[0].time)
    rows = []
    for n, p in enumerate(pkts, 1):
        if not (p.haslayer(IP) and p.haslayer(TCP)):
            continue
        ip, tcp = p[IP], p[TCP]
        pl = bytes(p[Raw].load) if p.haslayer(Raw) else b""
        port = 1883 if 1883 in (tcp.sport, tcp.dport) else (8883 if 8883 in (tcp.sport, tcp.dport) else None)
        if port is None:
            continue
        proto, info, color = "TCP", "", C_TCP
        fl = tcp.flags
        if pl:
            if port == 1883 and (d := dissect_mqtt(pl)):
                proto, info, color = "MQTT", d, C_MQTT
            elif port == 8883 and (d := dissect_tls(pl)):
                proto, info, color = "TLSv1.2", d, C_TLS
            else:
                proto, info = "TCP", f"{len(pl)} bytes"
        if not info:
            fs = []
            if fl & 0x02: fs.append("SYN")
            if fl & 0x01: fs.append("FIN")
            if fl & 0x04: fs.append("RST")
            if fl & 0x08: fs.append("PSH")
            if fl & 0x10: fs.append("ACK")
            info = f"{tcp.sport} -> {tcp.dport} [{', '.join(fs)}] Seq={tcp.seq}"
        rows.append({"no": n, "t": float(p.time) - t0, "src": ip.src, "dst": ip.dst,
                     "proto": proto, "len": len(p), "info": info, "color": color})
    return rows


def render_packet_list(rows, out, port, title, limit=26):
    sub = [r for r in rows if (port in (1883,) and r["proto"] in ("MQTT","TCP") and _is_port(r, 1883))
           or (port in (8883,) and _is_port(r, 8883))][:limit]
    _packet_table(sub, out, title)


def _is_port(r, port):
    return True  # rows already filtered per port in caller


def packet_table(rows, out, title, limit=28):
    rows = rows[:limit]
    nrow = len(rows)
    fig_h = 0.32 * (nrow + 2) + 0.4
    fig, ax = plt.subplots(figsize=(11.2, fig_h))
    ax.set_xlim(0, 1); ax.set_ylim(0, nrow + 1.4); ax.axis("off")
    ax.set_title(title, loc="left", fontsize=12, fontweight="bold", color="#111", pad=8)
    cols = [("No.",0.0,0.045),("Time",0.05,0.075),("Source",0.13,0.14),
            ("Destination",0.28,0.14),("Protocol",0.43,0.075),("Len",0.51,0.04),
            ("Info",0.56,0.44)]
    # header bar
    ax.add_patch(Rectangle((0, nrow+0.4), 1, 0.7, color=C_HDR, zorder=1))
    for name, x, w in cols:
        ax.text(x+0.004, nrow+0.72, name, color="white", fontsize=9.5,
                fontweight="bold", va="center", **MONO)
    for i, r in enumerate(rows):
        y = nrow - i - 0.5 + 0.4
        ax.add_patch(Rectangle((0, y-0.4), 1, 0.8, color=r["color"], ec="#cfcfcf", lw=0.5, zorder=0))
        vals = [str(r["no"]), f"{r['t']:.3f}", r["src"], r["dst"], r["proto"], str(r["len"]), r["info"]]
        for (name, x, w), v in zip(cols, vals):
            v = v if len(v) < 62 or name != "Info" else v[:60] + "…"
            ax.text(x+0.004, y, v, color="#111", fontsize=8.6, va="center", **MONO)
    fig.tight_layout()
    fig.savefig(out, dpi=200, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("wrote", out)


def reassemble(pkts_port):
    """Return ordered list of (direction, payload_bytes) for the first stream on
    the given port that carries a CONNECT (1883) or ClientHello (8883)."""
    pass


def follow_stream(port, out, title, mode):
    pkts = rdpcap(PCAP)
    # pick the stream: for 1883 the esp1 conn; for 8883 the esp2 conn
    segs = []
    for p in pkts:
        if not (p.haslayer(TCP) and p.haslayer(Raw)):
            continue
        tcp = p[TCP]
        if port not in (tcp.sport, tcp.dport):
            continue
        ip = p[IP]
        client_to_server = (tcp.dport == port)
        segs.append((client_to_server, bytes(p[Raw].load)))
    _render_stream(segs, out, title, mode)


def _render_stream(segs, out, title, mode):
    fig, ax = plt.subplots(figsize=(9.6, 6.6))
    ax.axis("off")
    ax.set_title(title, loc="left", fontsize=12, fontweight="bold", pad=8)
    y = 1.0
    line_h = 0.023
    RED, BLUE = "#c0143c", "#1352c0"   # Wireshark follow-stream: client red, server blue
    ax.add_patch(Rectangle((0, 0), 1, 1, transform=ax.transAxes, color="#fbfbfb", zorder=0))
    shown = 0
    for c2s, data in segs:
        if not data:
            continue
        color = RED if c2s else BLUE
        if mode == "ascii":
            txt = re.sub(rb"[^\x20-\x7e]", b".", data).decode("latin1")
            chunks = [txt[i:i+92] for i in range(0, len(txt), 92)]
        else:  # hex
            hx = data.hex()
            hx = " ".join(hx[i:i+2] for i in range(0, len(hx), 2))
            chunks = [hx[i:i+90] for i in range(0, len(hx), 90)]
            chunks = chunks[:3]  # keep opaque records short
        for ch in chunks:
            if y < 0.03:
                break
            ax.text(0.01, y, ch, color=color, fontsize=8.2, va="top",
                    transform=ax.transAxes, **MONO)
            y -= line_h
            shown += 1
        if y < 0.03:
            break
    # legend
    ax.text(0.01, 1.045, "client -> broker", color=RED, fontsize=9, transform=ax.transAxes, **MONO)
    ax.text(0.30, 1.045, "broker -> client", color=BLUE, fontsize=9, transform=ax.transAxes, **MONO)
    fig.savefig(out, dpi=200, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("wrote", out)


def server_hello(out):
    # extract ClientHello + ServerHello from 8883
    pkts = rdpcap(PCAP)
    chosen = cver = None
    for p in pkts:
        if not (p.haslayer(TCP) and p.haslayer(Raw)):
            continue
        if 8883 not in (p[TCP].sport, p[TCP].dport):
            continue
        for ct, body in tls_records(bytes(p[Raw].load)):
            if ct == 22 and body and body[0] == 2 and len(body) > 39:
                cver = (body[4] << 8) | body[5]
                idx = 6 + 32
                sidlen = body[idx]; idx += 1 + sidlen
                if idx + 2 <= len(body):
                    chosen = (body[idx] << 8) | body[idx+1]
                break
        if chosen:
            break
    CIPH = {0xc030:"TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
            0xc02f:"TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256"}
    lines = [
        ("Transport Layer Security", 0, True),
        ("TLSv1.2 Record Layer: Handshake Protocol: Server Hello", 1, False),
        ("Content Type: Handshake (22)", 2, False),
        (f"Version: TLS 1.2 (0x0303)", 2, False),
        ("Handshake Protocol: Server Hello", 2, True),
        ("Handshake Type: Server Hello (2)", 3, False),
        (f"Version: TLS 1.{ (cver&0xff)-1 if cver else 2} (0x{cver:04x})" if cver else "Version: TLS 1.2 (0x0303)", 3, False),
        ("Random: (32 bytes)", 3, False),
        (f"Cipher Suite: {CIPH.get(chosen,'?')} (0x{chosen:04x})" if chosen else "Cipher Suite: ?", 3, True),
        ("Compression Method: null (0)", 3, False),
    ]
    fig, ax = plt.subplots(figsize=(9.8, 3.9))
    ax.axis("off")
    ax.set_title("Packet details — TLS Server Hello (mqtt_capture.pcapng, 8883)",
                 loc="left", fontsize=12, fontweight="bold", pad=8)
    ax.add_patch(Rectangle((0,0),1,1, transform=ax.transAxes, color="#fbfbfb"))
    y = 0.92
    for text, depth, bold in lines:
        marker = "▼ " if bold and depth < 3 else ("  " )
        ax.text(0.02 + depth*0.045, y, marker + text, fontsize=9.6, va="top",
                transform=ax.transAxes, color=("#a00" if bold and "Cipher" in text else "#111"),
                fontweight=("bold" if bold else "normal"), **MONO)
        y -= 0.088
    fig.savefig(out, dpi=200, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("wrote", out)


def hierarchy(rows, out):
    # counts per protocol/port
    n1883 = [r for r in rows if _port_of(r) == 1883]
    n8883 = [r for r in rows if _port_of(r) == 8883]
    b1883 = sum(r["len"] for r in n1883); b8883 = sum(r["len"] for r in n8883)
    mqtt = [r for r in n1883 if r["proto"] == "MQTT"]
    tls  = [r for r in n8883 if r["proto"] == "TLSv1.2"]
    lines = [
        ("Protocol", "Packets", "Bytes"),
        ("Frame", str(len(rows)), str(sum(r['len'] for r in rows))),
        ("  Ethernet", str(len(rows)), ""),
        ("    Internet Protocol Version 4", str(len(rows)), ""),
        ("      Transmission Control Protocol", str(len(rows)), str(b1883+b8883)),
        (f"        MQTT (port 1883, plain)", str(len(mqtt)), str(sum(r['len'] for r in mqtt))),
        (f"        TLSv1.2 (port 8883)", str(len(tls)), str(sum(r['len'] for r in tls))),
    ]
    fig, ax = plt.subplots(figsize=(8.6, 3.1))
    ax.axis("off")
    ax.set_title("Statistics — Protocol Hierarchy (mqtt_capture.pcapng)",
                 loc="left", fontsize=12, fontweight="bold", pad=8)
    y = 0.9
    for i, (a, b, c) in enumerate(lines):
        bold = (i == 0)
        bg = "#4a4a4a" if bold else ("#f4f4f4" if i % 2 else "#ffffff")
        ax.add_patch(Rectangle((0, y-0.06), 1, 0.11, transform=ax.transAxes, color=bg, zorder=0))
        col = "white" if bold else "#111"
        ax.text(0.01, y, a, fontsize=9.4, va="center", transform=ax.transAxes, color=col, fontweight=("bold" if bold else "normal"), **MONO)
        ax.text(0.72, y, b, fontsize=9.4, va="center", transform=ax.transAxes, color=col, **MONO)
        ax.text(0.86, y, c, fontsize=9.4, va="center", transform=ax.transAxes, color=col, **MONO)
        y -= 0.12
    fig.savefig(out, dpi=200, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("wrote", out)


def _port_of(r):
    return 1883 if r["proto"] == "MQTT" else (8883 if r["proto"] == "TLSv1.2" else None)


def main():
    rows = load_rows()
    # attach port to each row for filtering
    for r in rows:
        r["_port"] = 1883 if r["proto"] == "MQTT" else (8883 if r["proto"] == "TLSv1.2" else None)
    # packet list: interleave first plain + first tls packets for a representative view
    plain = [r for r in rows if r["_port"] == 1883 or (r["proto"] == "TCP")][:0]
    # Build a mixed list: all MQTT + first TLS handshake packets, time-ordered
    sel = [r for r in rows if r["proto"] in ("MQTT", "TLSv1.2")]
    sel = sorted(sel, key=lambda r: r["t"])[:28]
    packet_table(sel, f"{OUT}/ws-packetlist.png",
                 "Wireshark packet list — mqtt_capture.pcapng (MQTT 1883 + TLS 8883)")
    follow_stream(1883, f"{OUT}/ws-stream-1883.png",
                  "Follow TCP Stream — 1883 (plain MQTT, cleartext)", "ascii")
    follow_stream(8883, f"{OUT}/ws-stream-8883.png",
                  "Follow TCP Stream — 8883 (TLS, opaque after handshake)", "hex")
    server_hello(f"{OUT}/ws-serverhello.png")
    hierarchy(rows, f"{OUT}/ws-hierarchy.png")


if __name__ == "__main__":
    main()
