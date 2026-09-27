#!/bin/bash

# ──────────────────────────────────────────────
#  build.sh — Compila un plugin DPF via hvcc
#  Uso: ./build.sh <nome_plugin>
# ──────────────────────────────────────────────

set -euo pipefail

PLUGIN_NAME="${1:?Uso: ./build.sh <nome_plugin> [cartella_output]}"
PLUGIN_DIR="./src/${PLUGIN_NAME}"
OUTPUT_DIR="${2:-}"

if [[ ! -d "$PLUGIN_DIR" ]]; then
    echo "Errore: la cartella '$PLUGIN_DIR' non esiste."
    exit 1
fi

# ── 1. Genera i file DPF con hvcc ────────────

echo ">> hvcc: generazione plugin..."
cd "$PLUGIN_DIR"
hvcc "${PLUGIN_NAME}.pd" -g dpf -m plugin.json
cd - > /dev/null

# ── 1b. Header con gli indici dei parametri (sempre allineato al plugin) ──
SRC_DIR="${PLUGIN_DIR}/plugin/source"
DSP_HPP="${SRC_DIR}/HeavyDPF_${PLUGIN_NAME}.hpp"
{
    echo "// Generato da build.sh a partire da HeavyDPF_${PLUGIN_NAME}.hpp: NON modificare"
    echo "#pragma once"
    grep "#define HV_DPF_NUM_PARAMETER" "$DSP_HPP"
    sed -n '/enum Parameters/,/};/p' "$DSP_HPP"

    echo "// Valori di default, nello stesso ordine dell'enum"
    echo "static const float kParamDefaults[HV_DPF_NUM_PARAMETER] = {"
    sed -n 's/^[[:space:]]*_parameters\[[0-9][0-9]*\] = \(.*\);$/    \1,/p' "${SRC_DIR}/HeavyDPF_${PLUGIN_NAME}.cpp"
    echo "};"
} > "${SRC_DIR}/HeavyParams.hpp"

# ── 1c. UI custom (se presente) ───────────────
if [[ -d "${PLUGIN_DIR}/ui" ]]; then
    echo ">> UI custom: copio ${PLUGIN_DIR}/ui/ in plugin/source/"
    cp "${PLUGIN_DIR}"/ui/* "${SRC_DIR}/"
fi

# ── 2. Compila ────────────────────────────────

echo ">> Make: compilazione plugin..."
make -C "${PLUGIN_DIR}/plugin/source"

echo ""
echo "Build completato: ${PLUGIN_NAME}"

# ── 2b. Copia nella cartella di output (opzionale) ─────
if [[ -n "$OUTPUT_DIR" ]]; then
    mkdir -p "$OUTPUT_DIR"
    for bundle in "${PLUGIN_DIR}"/bin/*.vst3; do
        [[ -e "$bundle" ]] || continue
        name="$(basename "$bundle")"
        rm -rf "${OUTPUT_DIR:?}/${name:?}"
        cp -R "$bundle" "$OUTPUT_DIR/"
        echo ">> Installato: $OUTPUT_DIR/$name"
    done
fi

# ── 3. Cleanup cartella dep 1 livello sopra ────────────────────────────────

if [[ -d "$(pwd)/../dep/dpf-widgets" ]]; then
    echo ">> Cleanup: rimossa cartella dpf-widgets spuria..."
    rm -rf "$(pwd)/../dep/dpf-widgets"
    rmdir "$(pwd)/../dep" 2>/dev/null || true
fi