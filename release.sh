#!/bin/bash

# ──────────────────────────────────────────────────────────────────
#  release.sh — avvia su GitHub la release dei plugin elencati in release.conf
#
#  Uso:  ./release.sh <versione>        es. ./release.sh 1.0.0
#
#  Prima di lanciarlo:
#    1. release.conf (privato, non su git): nome del pacchetto e plugin da includere
#    2. release/notes/v<versione>.md: le note della versione (parti da TEMPLATE.md)
#    3. commit e push: GitHub compila quello che c'è sul repository, non i file locali
#
#  La compilazione (macOS, Windows, Linux), lo zip e la release avvengono su
#  GitHub Actions: vedi .github/workflows/release.yml
# ──────────────────────────────────────────────────────────────────

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

die() { echo "Errore: $*" >&2; exit 1; }

VERSION="${1:-}"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "indica la versione nel formato X.Y.Z, es. ./release.sh 1.0.0"

# ── Lista privata dei plugin ─────────────────────────────────────

[[ -f release.conf ]] || die "manca release.conf: copia release.conf.example in release.conf e scegli i plugin."
source release.conf

[[ -n "${RELEASE_NAME:-}" ]]    || die "RELEASE_NAME vuoto in release.conf."
[[ -n "${RELEASE_PLUGINS:-}" ]] || die "RELEASE_PLUGINS vuoto in release.conf."

for p in $RELEASE_PLUGINS; do
    [[ -f "src/$p/$p.pd" ]] || die "plugin '$p' non trovato (manca src/$p/$p.pd)."
    git ls-files --error-unmatch "src/$p/$p.pd" > /dev/null 2>&1 \
        || die "il plugin '$p' non è nel repository: fai commit e push."
done

# ── Note della versione ──────────────────────────────────────────

NOTES="release/notes/v$VERSION.md"
[[ -f "$NOTES" ]] || die "scrivi le note in $NOTES (parti da release/notes/TEMPLATE.md)."
git ls-files --error-unmatch "$NOTES" > /dev/null 2>&1 || die "$NOTES non è nel repository: fai commit e push."

# ── Il repository su GitHub è aggiornato? ────────────────────────

# --ignore-submodules=dirty: le modifiche DENTRO un submodule (es. dep/hvcc) non arrivano
# comunque a GitHub; conta solo se cambia il commit del submodule.
git diff --quiet --ignore-submodules=dirty && git diff --cached --quiet --ignore-submodules=dirty \
    || die "ci sono modifiche non committate: GitHub compilerebbe la versione precedente."

git fetch --quiet
[[ "$(git rev-parse HEAD)" == "$(git rev-parse '@{u}')" ]] \
    || die "il tuo ultimo commit non è su GitHub (o GitHub ha commit che non hai): fai push/pull."

if gh release view "v$VERSION" > /dev/null 2>&1; then
    die "la release v$VERSION esiste già."
fi

# ── Avvio ────────────────────────────────────────────────────────

echo "Release v$VERSION"
echo "  pacchetto: $RELEASE_NAME-v$VERSION.zip"
echo "  plugin:    $RELEASE_PLUGINS"
echo "  note:      $NOTES"
echo ""
read -rp "Avvio la compilazione su GitHub? [s/N] " answer
[[ "$answer" == "s" || "$answer" == "S" ]] || { echo "Annullato."; exit 0; }

gh workflow run release.yml \
    -f version="$VERSION" \
    -f plugins="$RELEASE_PLUGINS" \
    -f name="$RELEASE_NAME"

echo ""
echo "Avviata. Segui l'avanzamento con:  gh run watch"
echo "oppure su GitHub, nella scheda Actions. Alla fine la release sarà nella scheda Releases."
