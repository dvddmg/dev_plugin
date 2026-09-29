#!/bin/bash

# ──────────────────────────────────────────────────────────────────
#  new.sh — crea un nuovo plugin in src/<nome> partendo da templates/plugin
#
#  Uso:  ./new.sh      (fa qualche domanda)
#
#  Crea:
#    src/<nome>/<nome>.pd                    patch di partenza (stereo, parametro "gain")
#    src/<nome>/plugin.json                  metadati per hvcc/DPF (dati da config.sh)
#    src/<nome>/ui/HeavyDPF_<nome>_UI.cpp    interfaccia basata su common/ui/PluginUIBase.hpp
# ──────────────────────────────────────────────────────────────────

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
source "$ROOT/config.sh"
TEMPLATE="$ROOT/templates/plugin"

die() { echo "Errore: $*" >&2; exit 1; }

# ── Domande ──────────────────────────────────────────────────────

read -rp "Nome del plugin (es. Orbita): " TITLE
[[ -n "$TITLE" ]] || die "il nome non può essere vuoto."

# Nome tecnico: minuscolo, spazi e trattini → underscore, solo a-z 0-9 _
SLUG=$(printf '%s' "$TITLE" | tr '[:upper:]' '[:lower:]' | tr ' -' '__' | tr -cd 'a-z0-9_')
[[ "$SLUG" =~ ^[a-z] ]] || die "il nome deve iniziare con una lettera."

PLUGIN_DIR="$ROOT/src/$SLUG"
[[ ! -e "$PLUGIN_DIR" ]] || die "src/$SLUG esiste già."

read -rp "Descrizione breve: " DESCRIPTION
DESCRIPTION="${DESCRIPTION:-$TITLE}"
DESCRIPTION="${DESCRIPTION//\"/\'}"   # niente virgolette doppie: finisce in JSON e in C++

# unique_id: 4 caratteri, proposto dal nome (prima lettera maiuscola)
SUGGESTED=$(printf '%s' "$TITLE" | tr -cd '[:alnum:]' | cut -c1-4)
while [[ ${#SUGGESTED} -lt 4 ]]; do
    SUGGESTED="${SUGGESTED}x"
done
SUGGESTED="$(printf '%s' "${SUGGESTED:0:1}" | tr '[:lower:]' '[:upper:]')${SUGGESTED:1}"

read -rp "unique_id, esattamente 4 caratteri [$SUGGESTED]: " UNIQUE_ID
UNIQUE_ID="${UNIQUE_ID:-$SUGGESTED}"
[[ ${#UNIQUE_ID} -eq 4 ]] || die "unique_id deve avere esattamente 4 caratteri."
if grep -qs "\"unique_id\": \"$UNIQUE_ID\"" "$ROOT"/src/*/plugin.json; then
    die "unique_id '$UNIQUE_ID' è già usato da un altro plugin."
fi

echo "Formati (separati da virgola, invio = solo vst3):"
echo "  1) vst3"
echo "  2) clap"
echo "  3) lv2"
echo "  4) au (solo macOS)"
read -rp "Scegli [1]: " FORMAT_INPUT
FORMAT_INPUT="${FORMAT_INPUT:-1}"

FORMATS=""
IFS=',' read -ra CHOICES <<< "$FORMAT_INPUT"
for choice in "${CHOICES[@]}"; do
    case "${choice// /}" in
        1) format="vst3" ;;
        2) format="clap" ;;
        3) format="lv2_sep" ;;
        4) format="au" ;;
        *) die "scelta non valida: $choice" ;;
    esac
    FORMATS="${FORMATS:+$FORMATS, }\"$format\""
done
FORMATS="[$FORMATS]"

# ── Copia del template ───────────────────────────────────────────

# Protegge i caratteri speciali di sed (\ | &) in un valore da sostituire
esc() { printf '%s' "$1" | sed -e 's/[\\|&]/\\&/g'; }

# Copia un file del template sostituendo i segnaposto @CHIAVE@
fill() {
    sed -e "s|@NAME@|$(esc "$SLUG")|g" \
        -e "s|@TITLE@|$(esc "$TITLE")|g" \
        -e "s|@DESCRIPTION@|$(esc "$DESCRIPTION")|g" \
        -e "s|@MAKER@|$(esc "$MAKER")|g" \
        -e "s|@BRAND_ID@|$(esc "$BRAND_ID")|g" \
        -e "s|@UNIQUE_ID@|$(esc "$UNIQUE_ID")|g" \
        -e "s|@HOMEPAGE@|$(esc "$HOMEPAGE")|g" \
        -e "s|@URI@|$(esc "$URI_BASE/$SLUG")|g" \
        -e "s|@LICENSE@|$(esc "$LICENSE")|g" \
        -e "s|@FORMATS@|$(esc "$FORMATS")|g" \
        "$1" > "$2"
}

mkdir -p "$PLUGIN_DIR/ui"
fill "$TEMPLATE/plugin.pd"       "$PLUGIN_DIR/$SLUG.pd"
fill "$TEMPLATE/plugin.json"     "$PLUGIN_DIR/plugin.json"
fill "$TEMPLATE/ui/PluginUI.cpp" "$PLUGIN_DIR/ui/HeavyDPF_${SLUG}_UI.cpp"

# ── Riepilogo ────────────────────────────────────────────────────

echo ""
echo "Plugin '$TITLE' creato in src/$SLUG/"
echo ""
echo "  $SLUG.pd                     patch di partenza"
echo "  plugin.json                  unique_id: $UNIQUE_ID   brand_id: $BRAND_ID   formati: $FORMATS"
echo "  ui/HeavyDPF_${SLUG}_UI.cpp   interfaccia"
echo ""
echo "Prossimi passi:"
echo "  1. modifica la patch src/$SLUG/$SLUG.pd"
echo "  2. ./build.sh $SLUG --install"
echo ""
