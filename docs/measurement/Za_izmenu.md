# Predlog izmena — Za_izmenu.md

Ovaj fajl sadrži koncentrisan, profesionalan set predloga i konkretnih izmena za dokumentaciju i kod iz direktorijuma `docs/measurement` (00–11). Cilj je da rad izgleda formalnije, robustnije i prikladno za akademsku ili tehničku publikaciju.

1. Sažetak promena (prioriteti)

Visok prioritet
- Jasno i sažeto formulisati doprinos i hipotezu u uvodu (2–3 rečenice).
- Dokumentovati i tehnički rešiti OTA problem sa GitHub redirect-ima; predložiti i implementirati praćenje HTTP `Location` header-a ili korišćenje GitHub API-ja za dobijanje finalnog asset URL-a.
- Dodati preporuku i plan za kriptografsko potpisivanje firmware-a i Secure Boot u sekciju Bezbednost.

Srednji prioritet
- Precizirati statističku proceduru: N (broj ponavljanja), način računanja mean/p95/p99, tretman outliera, intervale poverenja.
- U dokumentaciju ubaciti tačne verzije korišćenih alata i biblioteka (ESP-IDF, PlatformIO, Mosquitto, mbedTLS, OS verzija RPi).
- Automatizovati runbook pomoću skripte koja izvodi: gen-certs → build (fullclean) → flash → prikupljanje MEAS logova → tcpdump → rtt.py → parsing.

Niski prioritet
- Lektura i stil: eliminisati kolokvijalizme, osigurati uniformnost termina i skraćenica (definisati pri prvom pojavljivanju).
- Numerisati i potpisati sve figure i tabele, osigurati referenciranje u tekstu.
- Dodati sekciju „How to cite / reproducibility" sa listom artefakata (pcap, results.csv, skripte, binares).

2. Konkretne izmene koje mogu odmah da izvršim i commitujem

- Dodati fajl `docs/measurement/Za_izmenu.md` (ovaj fajl) — sadrži checklist i smernice.
- Implementirati patch u `components/common/src/ota.c` koji:
  - prati HTTP 3xx redirect-e prilikom preuzimanja OTA binarnog fajla,
  - kao fallback podržava upit GitHub Releases API da dobije finalni asset URL,
  - zabeleži greške i vraća koristan status (log + MQTT stanje) na `home/<board>/ota/state`.

- Dodati skriptu `docs/measurement/scripts/run_experiment.sh` koja:
  - proverava prerekvizite i verzije alata,
  - izvršava `./gen-certs.sh <IP>` u `docs/measurement/broker`, kopira `ca.crt`,
  - build-uje env-ove (`pio run -e esp2_temperature -t fullclean` itd.),
  - monitoriše serijski izlaz i prikuplja MEAS logove, pokreće tcpdump i rtt.py,
  - parsira MEAS CSV i sačuva rezultate u `docs/measurement/results-aggregated.csv`.

- Urediti `docs/measurement/PROFESSIONAL_STYLE.md` (ako želite) kako bi uključivao eksplicitne smernice za citiranje i reproducibilnost.

3. Predlozi za formalni jezički stil (primeri)

- Original: „OTA sa GitHub Releases trenutno *pada* jer ..."
  - Predlog: „On-device OTA sa GitHub Releases ne uspeva zbog višestrukih HTTP 302 preusmerenja; preporučuje se implementacija praćenja zaglavlja `Location` ili upotreba GitHub API-ja radi dobijanja konačnog URL-a za asset." 

- Original: „puca linkovanje"
  - Predlog: „dolazi do greške pri linkovanju (linker error) usled nedostajuće datoteke pri konfiguraciji build-a." 

4. Tehnički detalji za OTA patch (sažeto)

- Cilj: omogućiti on-device preuzimanje binarnog fajla i manifesta čak i ako GitHub isporučuje 302 preusmeravanja.
- Rešenje (opcije):
  - a) Proširiti HTTP klijent (`esp_http_client`) kako bi automatski pratio redirect (postaviti `ESP_HTTP_CLIENT_DEFAULT_REDIRECT_LIMIT` ili ručno pratiti `Location` iz odgovora i ponoviti zahtev).
  - b) Umesto direktnog `releases/latest/download/<asset>`, preuzeti manifest preko GitHub REST API (auth opcionalan za javne repoove) i iz njega ekstraktovati konačni `browser_download_url` — zatim preuzeti binarnu datoteku.
- Logika potvrde: manifest treba validirati (JSON schema) i zabeležiti `version` + `url` u `home/<board>/ota/state` pre pokušaja instalacije.

5. Checklist za commitove koje mogu napraviti (ako potvrdite)

- [ ] Dodati ovaj fajl `docs/measurement/Za_izmenu.md` (uradjeno).
- [ ] Napraviti novu granu `ota-redirect-fix` i implementirati patch u `components/common/src/ota.c`.
- [ ] Testirati lokalno koristeći skriptu iz `docs/measurement/broker/gen-certs.sh` i proveriti da device uspešno preuzima binar sa GitHub Releases.
- [ ] Kreirati PR sa opisom izmene i linkovima na reprodukcione artefakte.

6. Sledeći koraci — šta očekujem od vas

- Potvrdite da želite da implementiram OTA redirect patch (opcija A) i da izmene idu direktno na `master` (ne preporučujem direktan commit na master bez PR), ili da radim izmene u novoj grani i pripremim PR.
- Ako želite PR: navesti ime grane (predlog: `ota-redirect-fix`) i željeni opis PR-a.

---

Ako želite, odmah ću: (A) push-ovati ovaj fajl (već ću ga sačuvati) i kreirati novu granu `ota-redirect-fix` sa patch-om za praćenje redirect-a u OTA modulu, testirati i zatim napraviti PR; ili (B) samo izvršiti lekturu i stilizaciju `PROFESSIONAL_STYLE.md`. Navedite A ili B i da li želite PR ili direktan commit na master.
