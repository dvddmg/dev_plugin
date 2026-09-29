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

# ── 1b-bis. DearImGui compilato dentro la cartella del plugin ─────
# Il Makefile di hvcc compila ../../../../dep/.../DearImGui.cpp: con quei "../"
# l'oggetto finisce FUORI da build/ (in ~/Desktop/dep) ed è lo stesso per
# macOS, Windows e Linux. Lo sostituiamo con un file locale che lo include.
MAKEFILE="${SRC_DIR}/Makefile"
if grep -q "dpf-widgets/opengl/DearImGui.cpp" "$MAKEFILE"; then
    mkdir -p "${SRC_DIR}/widgets"
    echo '#include "DearImGui.cpp"' > "${SRC_DIR}/widgets/DearImGui_build.cpp"
    sed -i.bak 's|^FILES_UI += .*dpf-widgets/opengl/DearImGui\.cpp$|FILES_UI += widgets/DearImGui_build.cpp|' "$MAKEFILE"
    rm -f "${MAKEFILE}.bak"
fi

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