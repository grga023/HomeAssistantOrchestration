#!/usr/bin/env bash
# wireshark_analyze.sh - deep packet analysis of the MQTTS-overhead capture with
# tshark (Wireshark CLI). Forces MQTT dissection on 1883 and TLS on 8883.
# Usage: ./wireshark_analyze.sh mqtt_thesis.pcap
set -u
PCAP="${1:?usage: wireshark_analyze.sh <pcap>}"
D1="-d tcp.port==1883,mqtt"
D2="-d tcp.port==8883,tls"

echo "########## 1. PROTOCOL HIERARCHY (Statistics -> Protocol Hierarchy) ##########"
tshark -r "$PCAP" $D1 $D2 -q -z io,phs 2>/dev/null

echo; echo "########## 2. TCP CONVERSATIONS (Statistics -> Conversations) ##########"
tshark -r "$PCAP" -q -z conv,tcp 2>/dev/null

echo; echo "########## 3. PLAIN 1883 - MQTT DISSECTION (readable) ##########"
tshark -r "$PCAP" $D1 -Y 'mqtt' \
  -T fields -e frame.number -e ip.src -e mqtt.msgtype -e mqtt.topic -e mqtt.msg \
  -E header=y -E separator='  |  ' 2>/dev/null | head -40

echo; echo "########## 3b. CREDENTIALS IN CLEARTEXT (MQTT CONNECT on 1883) ##########"
tshark -r "$PCAP" $D1 -Y 'mqtt.msgtype==1' \
  -T fields -e frame.number -e mqtt.clientid -e mqtt.username -e mqtt.passwd 2>/dev/null

echo; echo "########## 4. TLS 8883 - ServerHello (negotiated version + cipher) ##########"
tshark -r "$PCAP" $D2 -Y 'tls.handshake.type==2' -V 2>/dev/null \
  | grep -iE 'Handshake Type:|Version:|Cipher Suite:' | head

echo; echo "########## 4b. TLS handshake message sequence ##########"
tshark -r "$PCAP" $D2 -Y 'tls.handshake' \
  -T fields -e frame.number -e ip.src -e ip.dst -e tls.handshake.type 2>/dev/null | head -30

echo; echo "########## 5. TLS 8883 - Application Data records (opaque payload) ##########"
tshark -r "$PCAP" $D2 -Y 'tls.record.content_type==23' \
  -T fields -e frame.number -e ip.src -e tls.record.length 2>/dev/null | head -20

echo; echo "########## 6. FOLLOW TCP STREAM - 1883 (cleartext proof) ##########"
S1=$(tshark -r "$PCAP" $D1 -Y 'mqtt.msgtype==1 && tcp.dstport==1883' -T fields -e tcp.stream 2>/dev/null | head -1)
echo "(stream $S1)"
tshark -r "$PCAP" -q -z follow,tcp,ascii,${S1:-0} 2>/dev/null | head -45

echo; echo "########## 7. FOLLOW TCP STREAM - 8883 (encrypted / opaque) ##########"
S2=$(tshark -r "$PCAP" $D2 -Y 'tls.handshake.type==1 && tcp.dstport==8883' -T fields -e tcp.stream 2>/dev/null | head -1)
echo "(stream $S2 - shown as hex; note absence of readable topics/creds)"
tshark -r "$PCAP" -q -z follow,tcp,hex,${S2:-0} 2>/dev/null | sed -n '1,20p'
