#!/bin/bash

# ──────────────────────────────────────────────
#  build.sh — Compila un plugin DPF via hvcc
#  Uso: ./build.sh <nome_plugin>
# ──────────────────────────────────────────────

set -euo pipefail

PLUGIN_NAME="${1:?Uso: ./build.sh <nome_plugin>}"
PLUGIN_DIR="./src/${PLUGIN_NAME}"

if [[ ! -d "$PLUGIN_DIR" ]]; then
    echo "Errore: la cartella '$PLUGIN_DIR' non esiste."
    exit 1
fi

# ── 1. Genera i file DPF con hvcc ────────────

echo ">> hvcc: generazione plugin..."
cd "$PLUGIN_DIR"
hvcc "${PLUGIN_NAME}.pd" -g dpf -m plugin.json
cd - > /dev/null


# ── 2. Compila ────────────────────────────────

echo ">> Make: compilazione plugin..."
make -C "${PLUGIN_DIR}/plugin/source"

echo ""
echo "Build completato: ${PLUGIN_NAME}"

# ── 3. Cleanup cartella dep 1 livello sopra ────────────────────────────────

if [[ -d "$(pwd)/../dep/dpf-widgets" ]]; then
    echo ">> Cleanup: rimossa cartella dpf-widgets spuria..."
    rm -rf "$(pwd)/../dep/dpf-widgets"
    rmdir "$(pwd)/../dep" 2>/dev/null || true
fi