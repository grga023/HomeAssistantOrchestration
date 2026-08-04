#!/usr/bin/env bash
#
# build-docx.sh — sklapa celu dokumentaciju (poglavlja 00–10) u jedan Word
# dokument (master-rad.docx).
#
# Lanac: Asciidoctor (-> self-contained HTML5) -> Pandoc (-> .docx).
# Slike (fig-*.png) i tabele/admonicije se prenose; interni link:*.adoc[] linkovi
# postaju običan tekst u Word-u (očekivano).
#
# Zavisnosti:
#   - asciidoctor   (gem install asciidoctor)
#   - pandoc        (apt install pandoc  /  https://pandoc.org/installing.html)
# Opciono:
#   - reference.docx  (Word predložak u ovom direktorijumu) za stilove/fontove;
#     ako postoji, koristi se automatski (--reference-doc).
#
# Upotreba:
#   cd docs/measurement && ./build-docx.sh [izlaz.docx]
#
set -euo pipefail
cd "$(dirname "$0")"

OUT="${1:-master-rad.docx}"
TITLE="Analiza implementacije enkriptiranog MQTT protokola (MQTTS) u Smart Home sustavu"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

# --- provera zavisnosti -------------------------------------------------------
for tool in asciidoctor pandoc; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "GREŠKA: '$tool' nije instaliran." >&2
    case "$tool" in
      asciidoctor) echo "  Instaliraj: gem install asciidoctor" >&2 ;;
      pandoc)      echo "  Instaliraj: sudo apt install pandoc  (ili https://pandoc.org/installing.html)" >&2 ;;
    esac
    exit 1
  fi
done

# --- redosled poglavlja (00..10), index se NE uključuje ----------------------
CHAPTERS=(
  00-implementacija.adoc
  01-overview.adoc
  02-firmware-plan.adoc
  03-broker-pki-tls.adoc
  04-methodology.adoc
  05-runbook.adoc
  06-reprodukcija-wireshark.adoc
  07-okruzenje-i-izvodjenje.adoc
  08-rezultati-i-analiza.adoc
  09-wireshark-analiza.adoc
  10-realni-podaci-uredjaja.adoc
)

# --- generiši master.adoc sa include-ovima -----------------------------------
MASTER="$BUILD_DIR/master.adoc"
{
  echo "= ${TITLE}"
  echo ":doctype: book"
  echo ":toc:"
  echo ":toclevels: 3"
  echo ":sectnums:"
  echo ":icons: font"
  echo ":imagesdir: ."
  echo
  echo ":leveloffset: +1"
  for ch in "${CHAPTERS[@]}"; do
    if [[ ! -f "$ch" ]]; then
      echo "GREŠKA: nedostaje poglavlje '$ch'." >&2; exit 1
    fi
    echo "include::${PWD}/${ch}[]"
    echo
  done
  echo ":leveloffset: -1"
} > "$MASTER"

# --- Asciidoctor -> HTML5 (self-contained) -----------------------------------
# HTML je robusniji ulaz za Pandoc od DocBook-a (izbegava probleme sa ugnježdenim
# inline markupom); slike se ugrađuju preko --resource-path.
HTML="$BUILD_DIR/master.html"
asciidoctor -b html5 -a imagesdir="$PWD" -a nofooter -o "$HTML" "$MASTER"

# --- Pandoc HTML -> DOCX ------------------------------------------------------
PANDOC_ARGS=(--from html --to docx --toc --resource-path "$PWD" -o "$OUT")
if [[ -f reference.docx ]]; then
  PANDOC_ARGS+=(--reference-doc reference.docx)
  echo "Koristim reference.docx za stilove."
fi
pandoc "${PANDOC_ARGS[@]}" "$HTML"

echo "OK: napravljen '$OUT' ($(du -h "$OUT" | cut -f1))."
