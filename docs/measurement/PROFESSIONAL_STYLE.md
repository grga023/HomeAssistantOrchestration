# HomeAssistantOrchestration — Profesionalni sažetak i uređeni stil (docs/measurement 00–11)

Ovaj dokument predstavlja jedinstveni, profesionalno uređen i stilistički usklađen prikaz sadržaja iz direktorijuma `docs/measurement` (datoteke 00–11). Namenjen je kao referentni materijal za čitanje, citiranje i uključivanje u završni rad ili tehničku dokumentaciju. Tekst je na srpskom jeziku, formalnog i stručnog registra.

## Sažetak

Projekat demonstrira modularan sistem pametne kuće zasnovan na Raspberry Pi (Home Assistant + Mosquitto) i dva ESP32 uređaja. Cilj mernog dela je kvantifikacija troškova uvođenja TLS zaštite (MQTTS) u poređenju sa nešifrovanim MQTT-om: memorijski overhead, vremenski overhead prilikom povezivanja (handshake), mrežni overhead po prenosu i uticaj na end-to-end latenciju izvršavanja komande (RTT).

Istovremeno, implementiran je centralizovan OTA mehanizam (HTTPS) koji omogućava bezbedno ažuriranje firmvera sa GitHub Releases i automatizovanu integraciju sa Home Assistant-om (MQTT update entitet). U radu su jasno dokumentovani i problemi reproducibilnosti i ograničenja (npr. GitHub redirect na asset-e koji on-device OTA trenutno ne prati).

## Struktura i ključne tačke po poglavljima

- 00 — Implementacija sistema
  - Detaljan opis arhitekture, build sistema (PlatformIO + ESP-IDF), zajedničke komponente (`components/common`) i centralne implementacije OTA mehanizma.
  - Naglašeno: OTA je integrisan centralno u `mqtt_wrap`; dual-OTA particije i automatski rollback obezbeđuju bezbednost uređaja.

- 01 — Pregled i arhitektura
  - Postavljanje hipoteze: TLS uvodi merljive troškove, ali RTT po komandi ostaje prihvatljiv (< 200 ms).
  - Definisanje uloga: `esp1_lights` (baseline, plain MQTT) i `esp2_temperature` (MQTTS/TLS).

- 02 — Plan izmena firmvera
  - Konkretne izmene: compile-time gejt `-DMQTT_TLS=1`, ugradnja `ca.crt`, izbor URI šeme `mqtts://` za TLS, te minimalne izmene u `mqtt_wrap.c` uz očuvanje javnog API-ja.
  - Praktčna napomena: embed certifikata preko CMake (`target_add_binary_data`) zahteva pažnju pri imenovanju i fullclean nakon promene CMake parametara.

- 03 — Broker, PKI i TLS
  - Uputstvo za generisanje CA i server sertifikata (OpenSSL), SAN (DNS + IP) konfiguraciju i Mosquitto podešavanje sa dva listenera (1883 i 8883).
  - Diskusija o verifikaciji: razlika između verifikacije lanca poverenja i poklapanja imena (CN vs SAN); preporuka: primarna merenja sa `skip_cert_common_name_check=true` za reproduktivnost i dodatni „full verification" run sa DNS-SAN.

- 04 — Metodologija merenja
  - Instrumentacija: `MEAS` CSV logovi iz firmware-a sa definisanim fazama i metrike: Heap (memorija), connect_span (vreme konekcije TLS/TCP), mrežni snimak (pcap/Wireshark) i end-to-end command RTT (rtt.py).
  - Princip: fer poređenje pomoću unakrsnog oduzimanja (esp2 − esp1) jer oba čvora koriste identičan kod osim TLS grane.

- 05 — Runbook
  - Korak-po-korak uputstvo za reprodukciju eksperimenta: generisanje sertifikata, konfiguracija brokera, kopiranje CA u firmware, build (sa obaveznim fullclean prilikom prve izmene), flešovanje i prikupljanje metrika.

- 06 — Reprodukcija i Wireshark
  - Praktična uputstva za tcpdump/Wireshark (snimanje na brokeru, filteri, dokaz enkripcije i prikaz TCP tokova).

- 07 — Okruženje i izvršenje
  - Detalji test okruženja: verzije softvera, cipher-suite, parametri mbedTLS i sistemska konfiguracija koja utiče na rezultate.

- 08 — Rezultati i analiza
  - Izmereni rezultati potvrđuju očekivanja: značajan jednokratni trošak TLS handshaka i zadržani memorijski overhead (~30–40 KB), ali zanemarljiv uticaj na p95 command-RTT (ispod 200 ms).

- 09 — Wireshark analiza
  - Prezentacija pcap snimaka i dokaza: čitljiv payload na 1883 nasuprot enkriptovanom sadržaju na 8883.

- 10 — Stvarni podaci uređaja
  - Dokumentovani praktični problemi (naročito OTA sa GitHub Releases zbog 302 redirect-a) i predložena rešenja (praćenje Location header-a, alternativni hosting ili upotreba GitHub API-ja za direktan asset URL).

- 11 — CPU i dijagnostika
  - Dodatna merenja resursa i dijagnostički pokazatelji sa preporukama za optimizaciju.

## Ključni numerički nalazi (rezime)

- Handshake (vreme uspostave veze, median):
  - Plain MQTT (1883): ~56 ms
  - MQTTS/TLS (8883): ~584 ms
  - Jednokratna razlika (handshake trošak): ~+528 ms

- Memorija (zadržani TLS overhead):
  - Plain MQTT: ~7,35 KB
  - MQTTS/TLS: ~40,65 KB
  - Delta: ~+33,3 KB (očekivano ~28–40 KB u planu)

- Overhead po PUBLISH poruci (on-wire): ~+29 B

- End-to-end command RTT (mean): ~158,7 ms (plain) vs ~159,9 ms (TLS) — razlika zanemarljiva; p95 ostaje ispod 200 ms.

> Zaključak iz numeričkih rezultata: TLS uvećava potrošnju memorije i značajno produžava početni handshake, ali ne narušava korisnički doživljaj u pogledu latencije komandi.

## Implementacija OTA i bezbednosna vrednost

- OTA je implementiran centralno u `components/common/src/ota.c` i integrisan u `mqtt_wrap` tako da uređaji ne zahtevaju izmene.
- Dual-OTA particije i bootloader rollback minimizuju rizik od trajnog „brickovanja" uređaja.
- Trenutno ograničenje: on-device HTTPS OTA ne prati GitHub-ove višestruke 302 redirect-e na signed `githubusercontent.com` asset — posledica je neuspeh preuzimanja na uređaju. Radna preporuka: pratiti Location header u OTA klijentu ili koristiti direktan hosting asset-a.
- Napomena o veriﬁkaciji: firmware nije kriptografski potpisan u ovom radu — preporučuje se uvođenje potpisa i Secure Boot-a za produkcijsku distribuciju.

## Reproducibilnost i automatizacija

Dokumentacija sadrži potpuni runbook i skripte (rtt.py, pcap_analyze.py, plot_results.py, assemble_docx.py). Predložene powere:

1. Automatizovati runbook u jednoj shell/Python skripti koja:
   - proverava prerekvisite,
   - generiše i postavlja sertifikate,
   - izvršava buildove i flash,
   - prikuplja MEAS logove i pcap,
   - pokreće rtt.py i agregira rezultate.

2. Dodati CI koji automatski gradi i objavljuje binarnu verziju na GitHub Releases (sa manifestom i verzionisanjem) i koji replicira analizu (lokalno ili u kontrolisanom okruženju) za regresione testove.

## Problemi, ograničenja i preporuke

- GitHub Releases OTA:
  - Problem: višestruki 302 redirect-i koji on-device `esp_https_ota` trenutno ne prati.
  - Preporuke: pratiti `Location` header u OTA preuzimanju; koristiti GitHub API da se dobije finalni asset URL; ili hostovati OTA binar na URL-u koji vraća direktan stream bez redirect-a.

- Bezbednost OTA:
  - Dodati kriptografsko potpisivanje firmware-a i, po mogućnosti, Secure Boot za produkcijsku upotrebu.

- Merenja memorije:
  - Zadržani delta (`pre_start − connected`) je valjana i preporučena kao headline. `esp_get_minimum_free_heap_size()` koristiti s oprezom (prikazuje kumulativni minimum od boot-a).

- Testiranje i verifikacija:
  - Preporučiti jednu dodatnu seriju merenja sa `skip_cert_common_name_check=false` i DNS-SAN (full verification) kako bi se dokumentovala kompletna verifikacija identiteta broker-a.

## Predlozi za dalje radove i poboljšanja

- Implementirati praćenje redirect-a u OTA modulu i/ili podršku za dohvat asset-a preko GitHub API-ja.
- Uvesti kriptografsko potpisivanje firmvera i Secure Boot za potpunu verifikaciju integriteta.
- Automatizovati kompletan eksperiment u reproducibilan skript (inkluzivno parsiranje MEAS CSV i generisanje grafova) i dodati u CI pipeline.
- Dodati jedinične testove za ključne parsimne i verifikacione funkcije (npr. manifest parsiranje, topic matching).

## Zaključak

Dokumentacija `docs/measurement` je sveobuhvatna, profesionalna i reproducibilna. Rad jasno i verodostojno kvantifikuje cenu uvođenja TLS-a na ograničenom hardveru i istovremeno implementira praktičan OTA mehanizam sa jakim inženjerskim odlukama (dual-OTA, rollback, centralizovana OTA logika). Predložene izmene (praćenje redirect-a, potpisi firmvera, automatizacija) su logičan sledeći korak ka produkcijskoj upotrebi.

---

_Ovaj dokument je generisan i sačuvan automatski iz sadržaja `docs/measurement` (fajlovi 00–11). Ako želite da prilagodim ton (kraći executive summary, više tehničkih detalja ili dodatne tabelarne prikaze rezultata), mogu odmah pripremiti izmene._
