#!/usr/bin/env python3
"""m3_driver.py - drive traffic during the METRIC 3 pcap capture.

1) Resets both ESP boards over serial -> fresh TCP/TLS handshake + MQTT CONNECT
   + retained discovery/state PUBLISHes (gives handshake bytes + the cleartext
   -vs- opaque encryption proof).
2) Publishes an IDENTICAL topic+payload over plain 1883 and TLS-1.2 8883 so the
   per-PUBLISH wire overhead can be measured on a matched message.
"""
import re, ssl, time
import serial
import paho.mqtt.client as mqtt

HOST = "192.168.0.10"
CA = "components/common/certs/ca.crt"
TEST_TOPIC = "test/meas"
TEST_PAYLOAD = "X" * 32            # MQTT PUBLISH = 2+2+9+32 = 45 B plain
N_TEST = 6

s = open("components/common/include/secrets.h").read()
g = lambda k: (re.search(r'#define\s+%s\s+"([^"]*)"' % k, s) or [None, ""])[1]
USER, PW = g("MQTT_USER"), g("MQTT_PASSWORD")


def reset(port):
    p = serial.Serial(port, 115200, timeout=1)
    p.setDTR(False); p.setRTS(True); time.sleep(0.12)
    p.setRTS(False); time.sleep(0.05); p.close()


def mkclient(cid):
    try:
        from paho.mqtt.enums import CallbackAPIVersion
        return mqtt.Client(CallbackAPIVersion.VERSION2, client_id=cid)
    except Exception:
        return mqtt.Client(client_id=cid)


def matched_publish(port, tls):
    c = mkclient(f"m3-{port}")
    if USER:
        c.username_pw_set(USER, PW)
    if tls:
        # Force TLS 1.2 to match the ESP32 mbedTLS channel; skip name check like device.
        c.tls_set(ca_certs=CA, tls_version=ssl.PROTOCOL_TLSv1_2)
        c.tls_insecure_set(True)
    c.connect(HOST, port, 30)
    c.loop_start()
    time.sleep(1.0)
    for _ in range(N_TEST):
        c.publish(TEST_TOPIC, TEST_PAYLOAD, qos=0)
        time.sleep(0.35)
    time.sleep(1.0)
    c.loop_stop(); c.disconnect()
    print(f"   matched publish done on {'TLS 8883' if tls else 'plain 1883'}")


print(">> resetting both boards (fresh connect + publishes)")
for port in ("/dev/ttyUSB0", "/dev/ttyUSB2"):
    reset(port)
print(">> waiting 15 s for reconnect + discovery/state publishes")
time.sleep(15)
print(">> matched identical-payload publishes (plain then TLS-1.2)")
matched_publish(1883, False)
matched_publish(8883, True)
print(">> driver done")
