#!/bin/bash

# ──────────────────────────────────────────────
#  Plugin Scaffold Generator (DPF/hvcc)
#  Creates a plugin dev folder inside ./src
# ──────────────────────────────────────────────

set -euo pipefail

# ── Patch hvcc template (dpf-widgets path fix) ─
sed -i '' 's|../../{{dpf_path}}dpf-widgets|../{{dpf_path}}dpf-widgets|g' \
    "$(dirname "$0")/dep/hvcc/hvcc/generators/c2dpf/templates/Makefile_plugin"


# ── Prompt ────────────────────────────────────

read -rp "Nome del plugin: " PLUGIN_NAME

if [[ -z "$PLUGIN_NAME" ]]; then
    echo "Errore: il nome del plugin non può essere vuoto."
    exit 1
fi

# ── Formato plugin (multi-select) ─────────────

echo "Formati del plugin (separati da virgola, es: 1,3):"
echo "  1) vst3"
echo "  2) au"
echo "  3) lv2"
echo "  4) clap"
read -rp "Scegli [1-4]: " FORMAT_INPUT

FORMATS=""
IFS=',' read -ra CHOICES <<< "$FORMAT_INPUT"
for choice in "${CHOICES[@]}"; do
    choice=$(echo "$choice" | tr -d ' ')
    case "$choice" in
        1) FORMATS="${FORMATS}\"vst3\"," ;;
        2) FORMATS="${FORMATS}\"au\"," ;;
        3) FORMATS="${FORMATS}\"lv2\"," ;;
        4) FORMATS="${FORMATS}\"clap\"," ;;
        *) echo "Scelta non valida: $choice"; exit 1 ;;
    esac
done

# Rimuovi virgola finale
FORMATS="${FORMATS%,}"

if [[ -z "$FORMATS" ]]; then
    echo "Errore: seleziona almeno un formato."
    exit 1
fi

# ── Descrizione ───────────────────────────────

read -rp "Descrizione breve: " DESCRIPTION
DESCRIPTION="${DESCRIPTION:-no description}"

# ── Variabili derivate ────────────────────────

# Slug: lowercase, spazi → underscore, solo alfanumerici e _
PLUGIN_SLUG=$(echo "$PLUGIN_NAME" | tr '[:upper:]' '[:lower:]' | tr ' ' '_' | tr -cd 'a-z0-9_')

# Developer: nome utente di sistema
DEVELOPER=$(whoami)

# Unique ID: primi 4 char alfanumerici del nome, camelCase-style
UNIQUE_ID=$(echo "$PLUGIN_NAME" | tr -cd '[:alnum:]' | cut -c1-4)
if [[ ${#UNIQUE_ID} -ge 1 ]]; then
    UNIQUE_ID="$(echo "${UNIQUE_ID:0:1}" | tr '[:upper:]' '[:lower:]')${UNIQUE_ID:1}"
fi
while [[ ${#UNIQUE_ID} -lt 4 ]]; do
    UNIQUE_ID="${UNIQUE_ID}x"
done

# Brand ID: primi 4 char alfanumerici del developer, capitalizzato
BRAND_ID=$(echo "$DEVELOPER" | tr -cd '[:alnum:]' | cut -c1-4)
if [[ ${#BRAND_ID} -ge 1 ]]; then
    BRAND_ID="$(echo "${BRAND_ID:0:1}" | tr '[:lower:]' '[:upper:]')$(echo "${BRAND_ID:1}" | tr '[:upper:]' '[:lower:]')"
fi
while [[ ${#BRAND_ID} -lt 4 ]]; do
    BRAND_ID="${BRAND_ID}x"
done

VERSION="1, 1, 1"

# ── Creazione cartella ────────────────────────

PLUGIN_DIR="./src/${PLUGIN_SLUG}"

if [[ -d "$PLUGIN_DIR" ]]; then
    echo "Errore: la cartella '$PLUGIN_DIR' esiste già."
    exit 1
fi

mkdir -p "$PLUGIN_DIR"

# ── File .pd vuoto ────────────────────────────

cat > "${PLUGIN_DIR}/${PLUGIN_SLUG}.pd" << 'PD'
#N canvas 0 0 450 300 12;
PD

# ── File JSON metadata ───────────────────────

# Costruisci array JSON con indentazione corretta
# Converte "vst3","au" → linee indentate con virgole (tranne l'ultima)
FORMATS_INDENTED=$(echo "$FORMATS" | tr ',' '\n' | sed 's/^[[:space:]]*//' | awk '{lines[NR]=$0} END {for(i=1;i<NR;i++) print "            " lines[i] ","; print "            " lines[NR]}')

cat > "${PLUGIN_DIR}/plugin.json" << JSON
{
    "name": "${PLUGIN_SLUG}",
    "nosimd": true,
    "dpf": {
        "dpf_path": "../../dep/",
        "enable_ui": true,
        "description": "${DESCRIPTION}",
        "maker": "${DEVELOPER}",
        "brand_id": "${BRAND_ID}",
        "unique_id": "${UNIQUE_ID}",
        "homepage": "https://www.davidebardi.com/",
        "plugin_uri": "https://www.davidebardi.com/",
        "version": "${VERSION}",
        "license": "GPL-3.0-or-later",
        "midi_input": 0,
        "midi_output": 0,
        "plugin_formats": [
${FORMATS_INDENTED}
        ]
    }
}
JSON

# ── Output ────────────────────────────────────

echo ""
echo "Plugin '${PLUGIN_NAME}' creato in ${PLUGIN_DIR}/"
echo ""
echo "  ${PLUGIN_SLUG}.pd       (canvas vuoto)"
echo "  plugin.json"
echo "    maker     : ${DEVELOPER}"
echo "    brand_id  : ${BRAND_ID}"
echo "    unique_id : ${UNIQUE_ID}"
echo "    version   : ${VERSION}"
echo "    formats   : [${FORMATS}]"
echo ""