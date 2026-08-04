#!/usr/bin/env python3
"""meas_capture.py - reset a board N times over serial and collect its MEAS
heap/timing samples (METRIC 1 + METRIC 2 of the MQTTS-overhead study).

Each cycle: pulse the auto-reset line (DTR/RTS), then read the boot log until the
low_water_5s MEAS line appears (or a deadline). Parses the single-tag "MEAS" CSV
rows emitted by components/common/src/mqtt_wrap.c. Writes one CSV row per cycle.
"""
import argparse, csv, sys, time
import serial


def reset(p):
    # Classic ESP auto-reset: DTR->IO0 (keep high=run), pulse RTS->EN low.
    p.setDTR(False)
    p.setRTS(True)
    time.sleep(0.12)
    p.setRTS(False)
    time.sleep(0.05)
    p.reset_input_buffer()


def read_cycle(p, deadline_s):
    """Collect MEAS fields for one boot. Returns dict or None."""
    vals = {}
    end = time.time() + deadline_s
    while time.time() < end:
        raw = p.readline().decode("utf-8", "replace")
        if "MEAS:" not in raw:
            continue
        body = raw.split("MEAS:", 1)[1].strip()
        f = [x.strip() for x in body.split(",")]
        if len(f) < 4:
            continue
        metric, phase = f[1], f[2]
        if metric == "time":
            vals[f"time_{phase}_ms"] = int(f[4]) if f[4] else None
        elif metric == "heap":
            vals[f"heap_{phase}_free"] = int(f[3]) if f[3] else None
            if phase == "connected" and len(f) > 5 and f[5]:
                vals["heap_connected_largest"] = int(f[5])
        elif metric == "heapdelta":
            vals["retained_delta"] = int(f[3]) if f[3] else None
        elif metric == "tlscfg":
            vals["tlscfg"] = phase
        if "heap_low_water_5s_free" in vals:
            break  # last line of interest
    return vals or None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--board", required=True)
    ap.add_argument("-n", "--count", type=int, default=30)
    ap.add_argument("--deadline", type=float, default=9.0)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    p = serial.Serial(args.port, 115200, timeout=1)
    cols = ["board", "cycle", "tlscfg", "time_connect_span_ms", "time_connect_total_ms",
            "heap_entry_free", "heap_pre_start_free", "heap_connected_free",
            "heap_connected_largest", "retained_delta", "heap_low_water_5s_free"]
    n_ok = 0
    with open(args.out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        for i in range(args.count):
            reset(p)
            v = read_cycle(p, args.deadline)
            if not v or "time_connect_span_ms" not in v:
                print(f"[{args.board}] cycle {i}: incomplete, retrying", file=sys.stderr)
                v = read_cycle(p, args.deadline) or {}
            row = {c: v.get(c, "") for c in cols}
            row["board"] = args.board
            row["cycle"] = i
            w.writerow(row)
            fh.flush()
            if v.get("time_connect_span_ms") is not None:
                n_ok += 1
            print(f"[{args.board}] cycle {i}: span={v.get('time_connect_span_ms')}ms "
                  f"retained={v.get('retained_delta')}B "
                  f"connected_free={v.get('heap_connected_free')}", flush=True)
    p.close()
    print(f"[{args.board}] done: {n_ok}/{args.count} valid -> {args.out}")


if __name__ == "__main__":
    main()
