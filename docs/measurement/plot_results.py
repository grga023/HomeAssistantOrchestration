#!/usr/bin/env python3
"""
plot_results.py — grafikoni rezultata za master rad "MQTTS overhead".

Čita `results.csv` (šablon koji popuniš IZMERENIM brojevima) i pravi 4 figure za
rad, sa colorblind-safe paletom (validirano: plava=plain, narandžasta=TLS):

  fig-heap.png       zadržani heap (KB): plain vs TLS  -> memorijski overhead
  fig-handshake.png  vreme konekcije/handshake (ms): plain vs TLS (JEDNOKRATNO)
  fig-rtt.png        command RTT (ms): plain vs TLS, sa linijom 200 ms (hipoteza)
  fig-packet.png     veličina poruke na žici (B): plain vs TLS

results.csv trenutno sadrži OČEKIVANE (placeholder) vrednosti da vidiš izgled;
zameni ih svojim izmerenim brojevima.

Zahteva: pip install matplotlib
Upotreba: python3 plot_results.py [results.csv] [--out-dir .]
"""
import argparse
import csv
import sys

import matplotlib
matplotlib.use("Agg")  # bez displeja; snima PNG
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

# --- Paleta (dataviz, validirano light mode) ---
PLAIN   = "#2a78d6"   # blue,  slot 1
TLS     = "#eb6834"   # orange, slot 2
INK     = "#0b0b0b"   # primary ink
INK2    = "#52514e"   # secondary ink
MUTED   = "#898781"   # axis/labels
GRID    = "#e1e0d9"   # hairline grid
SURFACE = "#fcfcfb"   # chart surface
THRESH  = "#d03b3b"   # status-critical: linija granice 200 ms

plt.rcParams.update({
    "font.family": "sans-serif",
    "font.sans-serif": ["Segoe UI", "DejaVu Sans", "Arial"],
    "font.size": 11,
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "text.color": INK,
    "axes.labelcolor": INK2,
    "xtick.color": MUTED,
    "ytick.color": MUTED,
    "axes.edgecolor": "#c3c2b7",
})


def load_results(path):
    rows = {}
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            rows[r["metric"]] = {
                "unit": r["unit"],
                "plain": float(r["plain"]),
                "tls": float(r["tls"]),
            }
    return rows


def _style_axis(ax):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.yaxis.grid(True, color=GRID, linewidth=1, zorder=0)
    ax.set_axisbelow(True)


def bar2(ax, plain, tls, unit, title, fmt="{:.0f}"):
    """Dvo-stubični magnitude grafikon: x-osa nosi identitet (ne samo boja)."""
    labels = ["Plain\n(esp1, 1883)", "MQTTS\n(esp2, 8883)"]
    vals = [plain, tls]
    bars = ax.bar(labels, vals, color=[PLAIN, TLS], width=0.55, zorder=3,
                  edgecolor=SURFACE, linewidth=1.5)
    _style_axis(ax)
    ax.set_ylabel(unit)
    ax.set_title(title, color=INK, fontsize=12, fontweight="bold", pad=12)
    ax.margins(y=0.18)
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v, "  " + fmt.format(v),
                ha="center", va="bottom", color=INK, fontsize=11,
                fontweight="bold")
    return bars


def annotate_delta(ax, plain, tls, unit):
    delta = tls - plain
    factor = (tls / plain) if plain else float("inf")
    txt = f"Δ = +{delta:.0f} {unit}"
    if plain:
        txt += f"  (×{factor:.1f})"
    ax.text(0.5, 0.94, txt, transform=ax.transAxes, ha="center", va="top",
            color=INK2, fontsize=10)


def fig_heap(res, out):
    m = res["heap_retained"]
    fig, ax = plt.subplots(figsize=(5, 4))
    bar2(ax, m["plain"], m["tls"], m["unit"],
         "Zadržani heap posle konekcije")
    annotate_delta(ax, m["plain"], m["tls"], m["unit"])
    fig.tight_layout()
    fig.savefig(out, dpi=200)
    plt.close(fig)


def fig_handshake(res, out):
    mean, p95 = res["handshake_mean"], res["handshake_p95"]
    fig, ax = plt.subplots(figsize=(5.2, 4))
    bar2(ax, mean["plain"], mean["tls"], mean["unit"],
         "Vreme konekcije / TLS handshake")
    # p95 markeri (kratke crte) preko stubića
    for x, key in ((0, "plain"), (1, "tls")):
        ax.plot([x - 0.27, x + 0.27], [p95[key], p95[key]],
                color=INK2, linewidth=1.5, zorder=4)
        ax.text(x, p95[key], f" p95 {p95[key]:.0f}", va="bottom", ha="left",
                color=INK2, fontsize=8)
    ax.text(0.5, 0.94, "jednokratni trošak pri povezivanju (ne po komandi)",
            transform=ax.transAxes, ha="center", va="top", color=INK2, fontsize=9)
    fig.tight_layout()
    fig.savefig(out, dpi=200)
    plt.close(fig)


def fig_rtt(res, out):
    mean, p95 = res["rtt_mean"], res["rtt_p95"]
    fig, ax = plt.subplots(figsize=(5.2, 4))
    bar2(ax, mean["plain"], mean["tls"], mean["unit"],
         "End-to-end command RTT")
    for x, key in ((0, "plain"), (1, "tls")):
        ax.plot([x - 0.27, x + 0.27], [p95[key], p95[key]],
                color=INK2, linewidth=1.5, zorder=4)
        ax.text(x, p95[key], f" p95 {p95[key]:.0f}", va="bottom", ha="left",
                color=INK2, fontsize=8)
    # Granica hipoteze
    top = max(mean["tls"], p95["tls"], 200) * 1.25
    ax.set_ylim(0, top)
    ax.axhline(200, color=THRESH, linewidth=1.5, linestyle="--", zorder=5)
    ax.text(1.48, 200, "200 ms (granica hipoteze)", color=THRESH, fontsize=9,
            va="bottom", ha="right")
    fig.tight_layout()
    fig.savefig(out, dpi=200)
    plt.close(fig)


def fig_packet(res, out):
    m = res["msg_wire_size"]
    fig, ax = plt.subplots(figsize=(5, 4))
    bar2(ax, m["plain"], m["tls"], m["unit"],
         "Veličina poruke na žici (po PUBLISH-u)")
    annotate_delta(ax, m["plain"], m["tls"], m["unit"])
    fig.tight_layout()
    fig.savefig(out, dpi=200)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="?", default="results.csv")
    ap.add_argument("--out-dir", default=".")
    args = ap.parse_args()

    try:
        res = load_results(args.csv)
    except FileNotFoundError:
        print(f"[!] Nema {args.csv}. Popuni results.csv izmerenim brojevima.",
              file=sys.stderr)
        sys.exit(1)

    d = args.out_dir.rstrip("/\\")
    fig_heap(res,      f"{d}/fig-heap.png")
    fig_handshake(res, f"{d}/fig-handshake.png")
    fig_rtt(res,       f"{d}/fig-rtt.png")
    fig_packet(res,    f"{d}/fig-packet.png")
    print("Napravljeno: fig-heap.png, fig-handshake.png, fig-rtt.png, fig-packet.png")


if __name__ == "__main__":
    main()
