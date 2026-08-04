#!/usr/bin/env python3
"""Capture ESP console over serial. Optionally hard-reset first to grab a full
boot log (ROM bootloader -> 2nd stage -> app -> Wi-Fi -> MQTT).

usage: cap_serial.py <port> <seconds> <outfile> [reset]
"""
import serial, sys, time

port, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
do_reset = len(sys.argv) > 4 and sys.argv[4] == "reset"

ser = serial.Serial(port, 115200, timeout=0.2)
if do_reset:
    # hard reset to RUN the app: pulse EN (RTS) while GPIO0 (DTR) stays HIGH
    ser.dtr = False      # GPIO0 = HIGH -> normal boot (not download mode)
    ser.rts = True       # EN = LOW  -> chip held in reset
    time.sleep(0.15)
    ser.rts = False      # EN = HIGH -> boot the application

start = time.time()
total = 0
with open(out, "wb") as f:
    while time.time() - start < secs:
        chunk = ser.read(4096)
        if chunk:
            f.write(chunk)
            f.flush()
            total += len(chunk)
ser.close()
print(f"captured {total} bytes from {port} -> {out}")
