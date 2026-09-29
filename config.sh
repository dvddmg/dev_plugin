# ──────────────────────────────────────────────────────────────────
#  config.sh — impostazioni comuni a tutti i plugin
#
#  Letto da new.sh (dati del produttore per plugin.json)
#  e da build.sh (cartelle di installazione, immagine Docker).
# ──────────────────────────────────────────────────────────────────

# Produttore: uguale per tutti i plugin
MAKER="davide"
BRAND_ID="Dvmg"                                   # esattamente 4 caratteri, almeno uno maiuscolo
HOMEPAGE="https://www.davidebardi.com/"
URI_BASE="https://www.davidebardi.com/plugins"    # plugin_uri = URI_BASE/<nome_plugin>
LICENSE="GPL-3.0-or-later"

# Cartelle in cui ./build.sh --install copia i VST3 (una per sistema)
INSTALL_DIR_MAC="/Library/Audio/Plug-Ins/VST3/davide"
INSTALL_DIR_WIN="/c/Program Files/Common Files/VST3/davide"
INSTALL_DIR_LINUX="$HOME/.vst3/davide"

# Immagine Docker usata per compilare la versione Linux (vedi docker/linux/Dockerfile)
DOCKER_IMAGE="dpf-linux"
