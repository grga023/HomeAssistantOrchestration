#!/usr/bin/env bash
#
# gen-certs.sh — lokalni CA + server sertifikat za MQTTS (master rad).
#
# Pravi:
#   ca.key      privatni ključ CA-a        (TAJNA — ostaje na RPi, ne u firmware)
#   ca.crt      javni CA sertifikat        (ide u firmware: components/common/certs/ca.crt)
#   server.key  privatni ključ brokera     (TAJNA — samo /etc/mosquitto/certs)
#   server.crt  server sertifikat sa SAN   (DNS:homeassistant.local + IP:<RPi-IP>)
#
# SAN sadrži I DNS I IP, pa podržava oba pristupa iz rada:
#   - skip_cert_common_name_check=true  (prikupljanje podataka; poklapanje po IP-u)
#   - full verification (skip=false)    (potvrdni run; pouzdano po DNS-u homeassistant.local)
#
# Upotreba:
#   ./gen-certs.sh <RPi-IP> [DNS-ime]
#   ./gen-certs.sh 192.168.1.10
#   ./gen-certs.sh 192.168.1.10 homeassistant.local
#
set -euo pipefail

IP="${1:-}"
DNS="${2:-homeassistant.local}"

if [[ -z "$IP" ]]; then
  echo "Upotreba: $0 <RPi-IP> [DNS-ime]" >&2
  echo "Primer:   $0 192.168.1.10 homeassistant.local" >&2
  exit 1
fi

echo ">> CA + server cert za IP=$IP  DNS=$DNS"

# 1) Lokalni CA (RSA-2048, SHA256, 10 god). CN je labela, ne proverava se protiv IP-a.
if [[ -f ca.key && -f ca.crt ]]; then
  echo ">> ca.key/ca.crt već postoje — preskačem generisanje CA (obriši ih za nov CA)."
else
  openssl genrsa -out ca.key 2048
  openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 \
    -subj "/CN=HomeLab-CA" -out ca.crt
  echo ">> Napravljen CA: ca.key (TAJNA), ca.crt (javni)."
fi

# 2) Server ključ — NEENKRIPTOVAN (bez -des3), inače Mosquitto visi čekajući lozinku.
openssl genrsa -out server.key 2048

# 3) CSR — CN = DNS ime (za lep prikaz); pravi identitet je u SAN-u.
openssl req -new -key server.key -subj "/CN=${DNS}" -out server.csr

# 4) SAN ekstenzija — I DNS I IP.
cat > san.ext <<EOF
subjectAltName = DNS:${DNS}, IP:${IP}
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
EOF

# 5) Potpiši server.crt CA-om, UBACUJUĆI SAN preko -extfile.
openssl x509 -req -in server.csr \
  -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 825 -sha256 -extfile san.ext

# 6) Provera: SAN je stvarno u sertifikatu + lanac valja.
echo
echo ">> Subject Alternative Name u server.crt:"
openssl x509 -in server.crt -noout -text | grep -A1 "Subject Alternative Name"
echo
echo ">> Provera lanca:"
openssl verify -CAfile ca.crt server.crt

# 7) Čišćenje privremenih fajlova.
rm -f server.csr san.ext ca.srl

cat <<EOF

>> GOTOVO. Fajlovi:
   ca.crt      -> u firmware:  cp ca.crt ../../../components/common/certs/ca.crt
   ca.crt      -> na broker:   /etc/mosquitto/certs/ca.crt   (chmod 644)
   server.crt  -> na broker:   /etc/mosquitto/certs/server.crt (chmod 644)
   server.key  -> na broker:   /etc/mosquitto/certs/server.key (chown root:mosquitto, chmod 640)

   NIKAD ne stavljaj ca.key ni server.key u firmware.
EOF
