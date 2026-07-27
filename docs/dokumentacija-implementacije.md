# HomeAssistantOrchestration — Dokumentacija implementacije

*Osnova za master rad. Opisuje arhitekturu, implementaciju i ključne inženjerske
odluke sistema za orkestraciju pametne kuće zasnovanog na ESP32, MQTT-u i Home
Assistant-u, sa posebnim osvrtom na implementaciju OTA (over-the-air)
ažuriranja firmvera.*

> Konverzija u Word: `pandoc docs/dokumentacija-implementacije.md -o master-rad.docx`

---

## 1. Uvod i cilj sistema

Sistem `HomeAssistantOrchestration` predstavlja demonstracioni sistem pametne
kuće izgrađen oko centralnog kontrolera **Home Assistant** (koji radi na Raspberry
Pi računaru) i **dva ESP32 mikrokontrolera** koji sa njim komuniciraju
isključivo preko **MQTT** protokola. Firmver je napisan u **čistom C jeziku** na
**ESP-IDF** razvojnom okviru i buildovan pomoću **PlatformIO** alata.

Cilj rada je bio da se izgradi modularan, proširiv sistem u kome:

1. svaki uređaj (ploča) obavlja jednu jasno definisanu ulogu,
2. uređaji dele zajednički sloj koda (mrežni stek, MQTT, Home Assistant
   integracija) umesto dupliranja logike,
3. novi uređaji i entiteti se dodaju po ustaljenom obrascu, i
4. firmver se može ažurirati **daljinski (OTA)** bez fizičkog pristupa uređaju.

Pored funkcionalnog dela, projekat sadrži i **mernu studiju** (u
`docs/measurement/`) koja poredi performanse običnog MQTT-a i MQTTS/TLS varijante
na ograničenom hardveru ESP32 — što je i motiv za zadržavanje baš dve ploče
(`esp1_lights` kao „plain" referenca i `esp2_temperature` kao TLS predmet
analize).

---

## 2. Pregled arhitekture

### 2.1 Topologija sistema

```
                 +-------------------------+
                 |  Raspberry Pi           |
                 |  Home Assistant + MQTT  |
                 +-----------+-------------+
                             | MQTT (Mosquitto broker)
              +--------------+--------------+
              |                             |
          ESP1 Lights                  ESP2 Temp
          relejna ploča                DHT22 + grejač
```

Centralni čvor je Raspberry Pi na kome rade Home Assistant i Mosquitto MQTT
broker. Svaki ESP32 se povezuje na WiFi mrežu, uspostavlja MQTT sesiju sa
brokerom i objavljuje svoje stanje i konfiguraciju. Home Assistant automatski
otkriva uređaje putem mehanizma **MQTT Discovery**.

### 2.2 Uređaji (ploče)

| Ploča | Env (PlatformIO) | Uloga | Hardver | HA entiteti |
|-------|------------------|-------|---------|-------------|
| ESP1 | `esp1_lights` | Osvetljenje | 2–4-kanalna relejna ploča | `switch` × N, `update` |
| ESP2 | `esp2_temperature` | Klima | DHT22 + opcioni relej grejača | `sensor` (temperatura, vlažnost), `switch` (grejač), `update` |

### 2.3 Softverski stek

- **ESP-IDF 5.4.0** (čist C), bez Arduino sloja
- **PlatformIO** (`framework = espidf`), toolchain espressif32
- **FreeRTOS** (nativni, deo ESP-IDF-a) za konkurentne zadatke
- **esp-mqtt** za MQTT klijent, **esp_wifi** (station režim) za mrežu
- **cJSON** za sastavljanje/parsiranje JSON poruka
- **mbedTLS** + **esp_https_ota** + javni CA bundle za bezbedan OTA prenos

---

## 3. Build sistem (netrivijalan i bitan za razumevanje)

Obe ploče žive u **jednom CMake projektu**, ali proizvode **po jedan firmver po
ploči**. Izbor aktivne ploče teče ovako:

1. `platformio.ini` za svaki env postavlja
   `board_build.cmake_extra_args = -DSMARTHOME_BOARD=<env>`.
2. `src/CMakeLists.txt` pomoću `GLOB` uzima **samo** izvorne fajlove aktivne
   ploče (`${SMARTHOME_BOARD}/src/*.c`) i dodaje samo njen `include/` direktorijum.

Tako se u svakom build-u kompajlira **tačno jedan** `app_main()`. Ovaj pristup je
neophodan jer ESP-IDF ignoriše PlatformIO-ov `build_src_filter`. Posledica:
dodavanje `.c` fajla u `src/<ploča>/src/` automatski ga uključuje; dodavanje nove
ploče znači nov direktorijum `src/<ploča>/` i nov `[env:<ploča>]` blok.

Deljena konfiguracija flash memorije i particione tabele nalazi se u
`sdkconfig.defaults` i `partitions.csv` (videti poglavlje 6). Generisani
`sdkconfig.*` fajlovi su u `.gitignore` i regenerišu se iz `sdkconfig.defaults`
pri svakom build-u — trajne izmene se rade u `sdkconfig.defaults`, ne u
generisanim fajlovima.

### Osnovne komande

```
pio run                                  # build obe ploče (default_envs)
pio run -e esp1_lights                   # build jedne ploče
pio run -e esp2_temperature -t upload    # build + flešovanje preko USB
pio device monitor -e esp2_temperature   # serijski monitor (115200)
```

---

## 4. Zajednička komponenta `common`

Sav deljeni kod je smešten u ESP-IDF komponentu `components/common/`, od koje
zavisi svaka ploča (`REQUIRES common` u `src/CMakeLists.txt`). Komponenta se
sastoji od pet modula:

### 4.1 `wifi_sta` — WiFi konektivnost
```c
void wifi_sta_start(const char *hostname);
bool wifi_sta_connected(void);
```
Inicijalizuje NVS, netif i event loop, povezuje se na pristupnu tačku iz
`secrets.h` u station režimu i postavlja hostname. Blokira dok se veza ne
uspostavi.

### 4.2 `mqtt_wrap` — MQTT klijent i centralna dispečerska tačka
```c
typedef void (*mqtt_msg_cb)(const char *topic, int topic_len,
                            const char *data, int data_len);
typedef void (*mqtt_conn_cb)(void);
void mqtt_start(const char *board, mqtt_msg_cb on_msg, mqtt_conn_cb on_conn);
int  mqtt_publish(const char *topic, const char *payload, bool retain);
int  mqtt_subscribe(const char *topic);
```
`mqtt_start()` konfiguriše klijent, postavlja **Last-Will** poruku (broker
objavljuje `offline`, retained, ako uređaj otkaže) i registruje event handler.
Na svakom (re)konektu objavljuje `online` i poziva `on_conn`. Ovaj modul je
takođe **centralna tačka za OTA** (videti poglavlje 7): pre prosleđivanja poruke
ploči, dispečer prvo daje priliku OTA sloju.

**Bitna konvencija:** dolazne MQTT poruke (`topic`/`data`) **nisu null-terminisane**
— svuda se koriste eksplicitne dužine (`topic_len`/`data_len`) i poređenje po
dužini (`strncmp`), nikad `strcmp`/`strstr` nad sirovim pokazivačem.

### 4.3 `ha_discovery` — Home Assistant auto-otkrivanje
```c
void ha_set_device(const char *board, const char *display_name);
void ha_publish_config(const char *component, const char *object_id,
                       const char *extra_json);
```
`ha_publish_config()` sastavlja i objavljuje (retained) konfiguracionu poruku na
temi `homeassistant/<component>/<board>/<object_id>/config`. Automatski ubacuje
`name`, `unique_id`, blok dostupnosti i **device blok** (uključujući
`sw_version` pročitan iz deskriptora aplikacije — verzija firmvera se time
prikazuje za svaki entitet). Pozivalac zadaje samo `extra_json` — komponentno
specifične ključeve, bez zagrada.

### 4.4 `app_rtos` — periodični FreeRTOS zadaci
```c
TaskHandle_t rtos_every_ms(const char *name, rtos_periodic_fn fn, void *ctx,
                           uint32_t period_ms, uint32_t stack,
                           UBaseType_t prio, BaseType_t core);
```
Pokreće bezdriftni periodični zadatak (`vTaskDelayUntil`) pripet na zadato jezgro.
Konvencije: aplikaciona logika na jezgru 1 (`RTOS_CORE_APP`), mreža na jezgru 0
(`RTOS_CORE_NET`).

### 4.5 `ota` — daljinsko ažuriranje firmvera
Novi modul (glavni doprinos ovog rada), detaljno opisan u poglavlju 7.

---

## 5. Home Assistant integracija

### 5.1 MQTT konvencije tema

| Funkcija | Komandna tema | Tema stanja |
|----------|---------------|-------------|
| Svetlo N | `home/lights/light<N>/set` | `home/lights/light<N>/state` |
| Temp./vlažnost | – | `home/temperature/sensor/state` (JSON) |
| Grejač | `home/temperature/heater/set` | `home/temperature/heater/state` |
| OTA (po ploči) | `home/<board>/ota/set`, `home/<board>/ota/install` | `home/<board>/ota/state` (JSON) |
| Dostupnost | – | `home/<board>/status` (retained, preko LWT) |

### 5.2 Obrazac po ploči

Svaka ploča je `main.c` (definicija pinova + jedan `*_start()` poziv, pa idle)
plus modul (`lights.c` odn. `climate.c`+`dht22.c`). Svaki `*_start()` prati isti
oblik:

1. inicijalizacija GPIO,
2. `wifi_sta_start(HOSTNAME)`,
3. `ha_set_device(BOARD, prikazno_ime)`,
4. `mqtt_start(BOARD, on_message, on_connect)`,
5. (ako čita hardver periodično) `rtos_every_ms(...)`.

`on_connect()` je idempotentan (izvršava se na svaki rekonekt): pretplaćuje se na
komandne teme, objavljuje HA discovery i trenutno stanje. `on_message()` uparuje
temu po dužini i primenjuje komandu.

---

## 6. Konfiguracija flash memorije i particija

OTA zahteva mogućnost čuvanja dve aplikativne slike (aktivna + nova). Prvobitna
šema (jedna aplikacija, 2 MB) to nije dozvoljavala, pa je uvedena **dual-OTA**
particiona tabela na **4 MB** flash-a (`partitions.csv`):

| Naziv | Tip | Podtip | Offset | Veličina |
|-------|-----|--------|--------|----------|
| nvs | data | nvs | 0x9000 | 0x4000 |
| otadata | data | ota | 0xd000 | 0x2000 |
| phy_init | data | phy | 0xf000 | 0x1000 |
| ota_0 | app | ota_0 | 0x10000 | 0x1B0000 (~1,7 MB) |
| ota_1 | app | ota_1 | 0x1C0000 | 0x1B0000 (~1,7 MB) |

Ključne postavke u `sdkconfig.defaults`:

- `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` — 4 MB flash
- `CONFIG_PARTITION_TABLE_CUSTOM=y` + `..._FILENAME="partitions.csv"`
- `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` — vraćanje na prethodnu sliku
- `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` — javni CA bundle za HTTPS

Verzija firmvera se drži u `version.txt` i ESP-IDF je ugrađuje u deskriptor
aplikacije (`esp_app_get_description()->version`), odakle je čitaju i OTA sloj i
`ha_discovery` (`sw_version`).

---

## 7. Implementacija OTA ažuriranja (glavni doprinos)

### 7.1 Zahtevi i odluke

| Aspekt | Odluka | Obrazloženje |
|--------|--------|--------------|
| Okidač | MQTT | Uklapa se u postojeći `on_connect`/`on_message` obrazac; ne uvodi nov protokol |
| Transport | HTTPS (`esp_https_ota`) | Šifrovan prenos; HTTP OTA je namerno isključen |
| Verifikacija | Javni CA bundle (`esp_crt_bundle`) | Nema ugrađenog sertifikata ni lokalnog servera; firmver se hostuje na GitHub Releases |
| Hosting | GitHub Releases | Stabilan `releases/latest/download` URL, javno dostupan |
| HA prikaz | `update` entitet | Prikazuje instaliranu vs najnoviju verziju + dugme „Install" |
| Bezbednost | Bootloader rollback | Nova slika se potvrđuje tek kad ponovo uspostavi MQTT vezu |

### 7.2 Centralno povezivanje (bez izmena u kodu ploča)

Pošto sav MQTT saobraćaj prolazi kroz `mqtt_wrap`, OTA je povezan **na jednom
mestu**, pa ploče (`lights.c`, `climate.c`, …) **ne zahtevaju nikakve izmene**:

- `mqtt_start()` poziva `ota_start(board)` (gradi OTA teme).
- U `MQTT_EVENT_CONNECTED`, posle korisničkog `on_conn()`, poziva se
  `ota_confirm_running_image()` (potvrda slike) pa `ota_on_connect()`
  (pretplata + HA discovery + dohvat manifesta).
- U `MQTT_EVENT_DATA` OTA dobija prvu priliku:
  ```c
  if (ota_handle_message(topic, topic_len, data, data_len)) break;
  s_on_msg(topic, topic_len, data, data_len);   /* inače prosledi ploči */
  ```

### 7.3 Javni API modula `ota`
```c
void ota_start(const char *board);            /* zapamti ploču, izgradi teme  */
void ota_on_connect(void);                    /* pretplata + HA update entity  */
bool ota_handle_message(const char*, int, const char*, int); /* obradi OTA temu */
void ota_confirm_running_image(void);         /* otkaži rollback ako radi       */
```

### 7.4 Tok ažuriranja

1. **Objava izdanja:** developer podigne `version.txt`, uradi `pio run`, i na
   GitHub Release okači, po ploči, binarnik `<board>.bin` i manifest
   `<board>.json` oblika `{"version":"1.0.1","url":"https://…/<board>.bin"}`.
2. **Otkrivanje:** na svakom MQTT rekonektu ploča dohvata `<base>/<board>.json`,
   saznaje najnoviju verziju i objavljuje stanje
   `{"installed_version":…,"latest_version":…}` na `home/<board>/ota/state`. Home
   Assistant tada nudi ažuriranje.
3. **Pokretanje:** korisnik klikne „Install" u HA (šalje se `INSTALL` na
   `home/<board>/ota/install`) — ili se ručno objavi URL na
   `home/<board>/ota/set`.
4. **Preuzimanje i upis:** OTA se izvršava u zasebnom FreeRTOS zadatku (da ne
   blokira MQTT petlju); `esp_https_ota()` preuzima sliku preko HTTPS i upisuje
   je u neaktivni slot.
5. **Restart i potvrda:** uređaj se restartuje na novu sliku. Nova slika je u
   stanju „pending verify"; tek kada ponovo uspostavi MQTT vezu, poziva se
   `esp_ota_mark_app_valid_cancel_rollback()`. Ako nova slika **ne uspe** da se
   poveže, bootloader je pri sledećem restartu automatski vraća na prethodnu —
   čime je uređaj zaštićen od „ciglanja" neispravnim firmverom.

### 7.5 Bezbednosna svojstva

- Prenos je **šifrovan (TLS)** i **autentifikovan** ka poznatim javnim CA
  telima (GitHub-ov lanac sertifikata).
- **Rollback** sprečava trajni kvar usled neispravne slike.
- Ograničenje: firmver se ne potpisuje kriptografski (nema Secure Boot), pa se
  poverenje oslanja na integritet HTTPS izvora i manifesta. To je prihvatljiv
  kompromis za demonstracioni/akademski kontekst i naznačeno je kao pravac
  daljeg rada.

---

## 8. Merna studija: MQTT vs MQTTS/TLS

Uporedo sa funkcionalnim sistemom, izvedena je merna studija (detaljno u
`docs/measurement/`) koja kvantifikuje cenu TLS-a na ESP32. Poređenje je između
**esp1** (plain MQTT, port 1883) i **esp2** (MQTTS/TLS, port 8883). Ključni
rezultati:

| Metrika | Plain MQTT | MQTTS/TLS | Razlika |
|---------|-----------|-----------|---------|
| Vreme uspostave veze (handshake) | ~60 ms | ~480 ms | ~8× (jednokratno) |
| Zadržana RAM/heap potrošnja | ~4 KB | ~36 KB | +32 KB (~9×) |
| Overhead po PUBLISH poruci | 42 B | 68 B | +26 B (~1,6×) |
| End-to-end RTT (komanda→odziv) | ~28 ms | ~45 ms | +17 ms |

Zaključak studije: iako TLS uvodi značajan jednokratni trošak (handshake,
memorija), **RTT u interaktivnoj kontroli ostaje nizak** (obe vrednosti ispod
hipotezne granice od 200 ms), pa je bezbednosni overhead prihvatljiv za primenu
u pametnoj kući.

> Napomena o statusu TLS-a u firmveru: MQTTS/TLS je u repozitorijumu opisan kao
> **projektovano rešenje** (`docs/measurement/02-firmware-plan.adoc`) i korišćen
> u mernom postavu; sam `mqtt_wrap.c` u trenutnoj verziji koristi plain
> `mqtt://` (1883). Uvezivanje TLS-a u firmverski put je naznačeno kao naredni
> korak.

---

## 9. Ograničenja i pravci daljeg rada

- **MQTTS/TLS u firmveru** — dovršiti wiring u `mqtt_wrap.c` (šema već postoji).
- **Kriptografski potpisan OTA / Secure Boot** — za produkcijski nivo poverenja.
- **Automatizovan release** (CI koji builduje i objavljuje `<board>.bin` +
  manifest).
- **Testovi** — `test/` je trenutno placeholder; jedinični testovi za logiku
  parsiranja poruka bili bi korisni.

---

## 10. Zaključak

Rad demonstrira modularan sistem pametne kuće u kome dve heterogene ESP32 ploče
dele jedinstven mrežni i integracioni sloj i besprekorno se uklapaju u Home
Assistant preko MQTT Discovery-ja. Glavni inženjerski doprinos je **OTA
podsistem** projektovan tako da bude potpuno transparentan za pojedinačne ploče:
implementiran jednom u zajedničkoj komponenti, sa bezbednim HTTPS prenosom i
automatskim rollback-om, primenjuje se na sve uređaje bez ijedne linije izmene u
njihovom kodu. Uz to, merna studija kvantifikuje cenu TLS zaštite na
ograničenom hardveru i pokazuje da je, uprkos jednokratnim troškovima,
interaktivna kontrola i dalje u prihvatljivim granicama latencije.

---

## Dodatak A — Struktura repozitorijuma

```
platformio.ini             2 env-a; izbor ploče preko cmake_extra_args
partitions.csv             dual-OTA tabela (ota_0/ota_1/otadata), 4 MB
sdkconfig.defaults         deljena IDF konfiguracija (4 MB, OTA, rollback, CA bundle)
version.txt                verzija firmvera -> esp_app_desc / HA sw_version
src/
  CMakeLists.txt           bira aktivnu ploču (-DSMARTHOME_BOARD=...)
  esp1_lights/{include,src}   lights.c + main.c
  esp2_temperature/{...}      climate.c, dht22.c + main.c
components/common/         zajednička IDF komponenta
  include/ + src/          wifi_sta, mqtt_wrap, ha_discovery, app_rtos, ota
docs/                      PlantUML dijagrami (boot + po ploči)
docs/measurement/          merna studija plain-vs-TLS (AsciiDoc)
```

## Dodatak B — Rečnik pojmova

- **OTA (Over-The-Air)** — daljinsko ažuriranje firmvera preko mreže.
- **MQTT Discovery** — mehanizam kojim Home Assistant automatski kreira entitete
  na osnovu retained konfiguracionih poruka.
- **Last-Will (LWT)** — poruka koju broker objavi u ime klijenta kada veza
  neočekivano padne (ovde: `offline` na `home/<board>/status`).
- **Rollback** — automatsko vraćanje na prethodnu firmver sliku ako se nova ne
  potvrdi kao ispravna.
- **Dual-OTA particije** — dva aplikativna slota (`ota_0`/`ota_1`) i `otadata`
  koji bira aktivni slot.
