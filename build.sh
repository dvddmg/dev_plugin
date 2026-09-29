#!/bin/bash

# ──────────────────────────────────────────────────────────────────
#  build.sh — genera e compila un plugin (patch Pd → hvcc → DPF)
#
#  Uso:  ./build.sh <plugin> [opzioni]
#
#  Piattaforme (senza opzioni: solo il sistema su cui stai lavorando)
#    --native     sistema attuale (predefinito)
#    --universal  macOS universal: Apple Silicon + Intel (solo da Mac)
#    --win        Windows 64 bit (da Mac/Linux con MinGW, nativo su Windows)
#    --linux      Linux 64 bit (da Mac/Windows con Docker, nativo su Linux)
#    --all        --universal + --win + --linux: bundle per la distribuzione
#
#  Altro
#    --install    copia il bundle VST3 nella cartella indicata in config.sh
#    --clean      cancella build e bin del plugin prima di compilare
# ──────────────────────────────────────────────────────────────────

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
source "$ROOT/config.sh"

# ── Utilità ──────────────────────────────────────────────────────

step() { echo ""; echo ">> $*"; }
die()  { echo "Errore: $*" >&2; exit 1; }

usage() {
    sed -n '6,17p' "$0" | sed 's/^# \{0,2\}//'
    exit 1
}

# Sistema su cui gira lo script
case "$(uname -s)" in
    Darwin)               HOST="mac" ;;
    MINGW*|MSYS*|CYGWIN*) HOST="win" ;;
    Linux)                HOST="linux" ;;
    *)                    HOST="unknown" ;;
esac

# ── Argomenti ────────────────────────────────────────────────────

PLUGIN_NAME="${1:-}"
[[ -n "$PLUGIN_NAME" && "$PLUGIN_NAME" != --* ]] || usage
shift

DO_NATIVE=false
DO_UNIVERSAL=false
DO_WIN=false
DO_LINUX=false
DO_INSTALL=false
DO_CLEAN=false

for arg in "$@"; do
    case "$arg" in
        --native)    DO_NATIVE=true ;;
        --universal) DO_UNIVERSAL=true ;;
        --win)       DO_WIN=true ;;
        --linux)     DO_LINUX=true ;;
        --all)       DO_UNIVERSAL=true; DO_WIN=true; DO_LINUX=true ;;
        --install)   DO_INSTALL=true ;;
        --clean)     DO_CLEAN=true ;;
        *)           echo "Opzione sconosciuta: $arg"; usage ;;
    esac
done

# Nessuna piattaforma indicata: compila per il sistema attuale
if ! $DO_NATIVE && ! $DO_UNIVERSAL && ! $DO_WIN && ! $DO_LINUX; then
    DO_NATIVE=true
fi

PLUGIN_DIR="src/${PLUGIN_NAME}"
SRC_DIR="${PLUGIN_DIR}/plugin/source"

[[ -d "$PLUGIN_DIR" ]]                  || die "la cartella '$PLUGIN_DIR' non esiste."
[[ -f "$PLUGIN_DIR/$PLUGIN_NAME.pd" ]]  || die "manca la patch '$PLUGIN_DIR/$PLUGIN_NAME.pd'."
[[ -f "$PLUGIN_DIR/plugin.json" ]]      || die "manca '$PLUGIN_DIR/plugin.json'."

# ── 1. Generazione dei sorgenti ──────────────────────────────────

# hvcc: usa quello già attivo oppure attiva il venv del progetto
ensure_hvcc() {
    command -v hvcc > /dev/null 2>&1 && return 0
    for act in "$ROOT/venv/bin/activate" "$ROOT/venv/Scripts/activate"; do
        if [[ -f "$act" ]]; then
            set +u; source "$act"; set -u
            break
        fi
    done
    command -v hvcc > /dev/null 2>&1 || die "hvcc non trovato: crea il venv e installa requirements.txt (vedi README)."
}

# HeavyParams.hpp: indici, default e info dei parametri, estratti dal codice di hvcc.
# tr -d '\r' serve su Windows, dove Python scrive i file con fine riga \r\n.
generate_params_header() {
    local hpp="$SRC_DIR/HeavyDPF_${PLUGIN_NAME}.hpp"
    local cpp="$SRC_DIR/HeavyDPF_${PLUGIN_NAME}.cpp"
    {
        echo "// Generato da build.sh a partire da HeavyDPF_${PLUGIN_NAME}.hpp/.cpp: NON modificare"
        echo "#pragma once"
        echo ""
        tr -d '\r' < "$hpp" | grep "#define HV_DPF_NUM_PARAMETER" || true
        tr -d '\r' < "$hpp" | sed -n '/enum Parameters/,/};/p'
        echo ""
        echo "// Valori di default, nello stesso ordine dell'enum"
        echo "static const float kParamDefaults[HV_DPF_NUM_PARAMETER] = {"
        tr -d '\r' < "$cpp" | sed -n 's/^[[:space:]]*_parameters\[[0-9][0-9]*\] = \(.*\);$/    \1,/p'
        echo "};"
        echo ""
        echo "// Nome, intervallo e tipo di ogni parametro, dall'initParameter() del plugin"
        echo "enum ParamFlags : unsigned { kParamOutput = 1u, kParamBool = 2u, kParamInt = 4u };"
        echo "struct ParamInfo { const char* name; float min; float max; unsigned flags; };"
        echo "static const ParamInfo kParamInfo[HV_DPF_NUM_PARAMETER] = {"
        tr -d '\r' < "$cpp" | awk '
            /^[[:space:]]*case param/        { inside = 1; name = ""; mn = "0.0f"; mx = "1.0f"; flags = "0" }
            inside && /parameter\.name =/    { split($0, q, "\""); name = q[2] }
            inside && /ranges\.min =/        { v = $3; sub(/;/, "", v); mn = v }
            inside && /ranges\.max =/        { v = $3; sub(/;/, "", v); mx = v }
            inside && /kParameterIsOutput/   { flags = flags " | kParamOutput" }
            inside && /kParameterIsBoolean/  { flags = flags " | kParamBool" }
            inside && /kParameterIsInteger/  { flags = flags " | kParamInt" }
            inside && /break;/               { printf "    { \"%s\", %s, %s, %s },\n", name, mn, mx, flags; inside = 0 }
        '
        echo "};"
    } > "$SRC_DIR/HeavyParams.hpp"
}

# Il Makefile di hvcc compila ../../../../dep/.../DearImGui.cpp: con quei "../" l'oggetto
# finisce FUORI dalla cartella di build ed è lo stesso per tutte le piattaforme.
# Lo sostituiamo con un file locale che lo include (in una sottocartella, così non
# viene preso anche dalla lista dei file DSP).
fix_imgui_makefile() {
    local makefile="$SRC_DIR/Makefile"
    grep -q "dpf-widgets/opengl/DearImGui.cpp" "$makefile" || return 0
    mkdir -p "$SRC_DIR/widgets"
    echo '#include "DearImGui.cpp"' > "$SRC_DIR/widgets/DearImGui_build.cpp"
    sed -i.bak 's|^FILES_UI += .*dpf-widgets/opengl/DearImGui\.cpp$|FILES_UI += widgets/DearImGui_build.cpp|' "$makefile"
    rm -f "$makefile.bak"
}

# UI custom: prima la base comune (common/ui), poi i file del plugin (src/<plugin>/ui)
copy_ui() {
    [[ -d "$PLUGIN_DIR/ui" ]] || return 0
    echo "   UI custom: common/ui + $PLUGIN_DIR/ui"
    cp "$ROOT"/common/ui/* "$SRC_DIR/"
    cp "$PLUGIN_DIR"/ui/* "$SRC_DIR/"
}

# Configurazione IntelliSense di VS Code: una per ogni plugin in src/ (solo su Mac)
update_vscode_config() {
    [[ "$HOST" == "mac" ]] || return 0
    local mode="macos-clang-arm64"
    [[ "$(uname -m)" == "x86_64" ]] && mode="macos-clang-x64"
    local first=true
    local json name

    mkdir -p "$ROOT/.vscode"
    {
        echo '// Generato da build.sh: una configurazione IntelliSense per ogni plugin in src/'
        echo '{'
        echo '    "env": {'
        echo '        "dpfIncludes": ['
        echo '            "${workspaceFolder}/dep/dpf/distrho",'
        echo '            "${workspaceFolder}/dep/dpf/dgl",'
        echo '            "${workspaceFolder}/dep/dpf-widgets/generic",'
        echo '            "${workspaceFolder}/dep/dpf-widgets/opengl",'
        echo '            "${workspaceFolder}/dep/dpf-widgets/opengl/DearImGui",'
        echo '            "${workspaceFolder}/common/ui"'
        echo '        ]'
        echo '    },'
        echo '    "configurations": ['
        for json in "$ROOT"/src/*/plugin.json; do
            [[ -e "$json" ]] || continue
            name="$(basename "$(dirname "$json")")"
            $first || echo '        },'
            first=false
            echo '        {'
            echo "            \"name\": \"$name\","
            echo '            "includePath": [ "${dpfIncludes}", "${workspaceFolder}/src/'"$name"'/plugin/source" ],'
            echo '            "defines": [ "DGL_OPENGL=1", "HAVE_OPENGL=1" ],'
            echo '            "compilerPath": "/usr/bin/clang++",'
            echo '            "cppStandard": "c++17",'
            echo "            \"intelliSenseMode\": \"$mode\""
        done
        $first || echo '        }'
        echo '    ],'
        echo '    "version": 4'
        echo '}'
    } > "$ROOT/.vscode/c_cpp_properties.json"
}

if $DO_CLEAN; then
    step "Pulizia: build e bin di $PLUGIN_NAME"
    rm -rf "${PLUGIN_DIR:?}"/build "${PLUGIN_DIR:?}"/build-* "${PLUGIN_DIR:?}/bin"
fi

ensure_hvcc

step "hvcc: genero i sorgenti da $PLUGIN_NAME.pd"
rm -rf "${PLUGIN_DIR:?}/plugin"   # hvcc non cancella i file vecchi: ripartiamo puliti
( cd "$PLUGIN_DIR" && hvcc "$PLUGIN_NAME.pd" -g dpf -m plugin.json )

generate_params_header
fix_imgui_makefile
copy_ui
update_vscode_config

# ── 2. Compilazione ──────────────────────────────────────────────
# Ogni piattaforma usa una cartella di build sua (BUILD_DIR_SUFFIX), ma tutte
# scrivono nello stesso bin/<plugin>.vst3: ognuna aggiunge la propria sottocartella.

build_native() {
    step "Compilo per il sistema attuale ($HOST)"
    make -C "$SRC_DIR"
}

build_universal() {
    [[ "$HOST" == "mac" ]] || die "--universal si può usare solo su macOS."
    step "Compilo per macOS universal (Apple Silicon + Intel)"
    make -C "$SRC_DIR" macos-universal-10.15 BUILD_DIR_SUFFIX=-universal
}

build_win() {
    if [[ "$HOST" == "win" ]]; then
        build_native
        return
    fi
    command -v x86_64-w64-mingw32-g++ > /dev/null 2>&1 \
        || die "manca MinGW: installalo con 'brew install mingw-w64'."
    step "Compilo per Windows 64 bit (MinGW)"
    make -C "$SRC_DIR" mingw64 BUILD_DIR_SUFFIX=-win64
}

build_linux() {
    if [[ "$HOST" == "linux" ]]; then
        build_native
        return
    fi
    command -v docker > /dev/null 2>&1 || die "manca Docker: installa e apri Docker Desktop."
    docker info > /dev/null 2>&1       || die "Docker non risponde: apri Docker Desktop e aspetta la balena ferma."
    if ! docker image inspect "$DOCKER_IMAGE" > /dev/null 2>&1; then
        step "Preparo l'immagine Docker '$DOCKER_IMAGE' (solo la prima volta)"
        docker build --platform linux/amd64 -t "$DOCKER_IMAGE" "$ROOT/docker/linux"
    fi
    step "Compilo per Linux 64 bit (Docker)"
    docker run --rm --platform linux/amd64 -v "$ROOT":/work -w /work "$DOCKER_IMAGE" \
        make -C "$SRC_DIR" BUILD_DIR_SUFFIX=-linux
}

if $DO_NATIVE;    then build_native;    fi
if $DO_UNIVERSAL; then build_universal; fi
if $DO_WIN;       then build_win;       fi
if $DO_LINUX;     then build_linux;     fi

# ── 3. Installazione ─────────────────────────────────────────────

install_bundles() {
    local dest bundle name
    case "$HOST" in
        mac)   dest="$INSTALL_DIR_MAC" ;;
        win)   dest="$INSTALL_DIR_WIN" ;;
        linux) dest="$INSTALL_DIR_LINUX" ;;
        *)     die "non so dove installare su questo sistema: copia il bundle a mano." ;;
    esac

    step "Installo in $dest"
    mkdir -p "$dest"
    for bundle in "$PLUGIN_DIR"/bin/*.vst3; do
        [[ -e "$bundle" ]] || continue
        name="$(basename "$bundle")"
        rm -rf "${dest:?}/${name:?}"
        cp -R "$bundle" "$dest/"
        echo "   installato: $dest/$name"
    done
}

if $DO_INSTALL; then install_bundles; fi

# ── Riepilogo ────────────────────────────────────────────────────

step "Build completato: $PLUGIN_NAME"
for bundle in "$PLUGIN_DIR"/bin/*.vst3; do
    [[ -e "$bundle" ]] || continue
    echo "   $(basename "$bundle") contiene:"
    for dir in "$bundle"/Contents/*/; do
        name="$(basename "$dir")"
        [[ "$name" == "Resources" ]] || echo "     - $name"
    done
done
