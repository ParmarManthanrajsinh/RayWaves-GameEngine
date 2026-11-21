#!/usr/bin/env bash
# Agent-loop smoke test: doctor -> lsp -> build -> run -> input -> validate.
# Mirrors scripts/agent-smoke.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
PRESET="zig-debug"
KEEP=0

while [ $# -gt 0 ]; do
    case "$1" in
        --preset|-p) PRESET=${2:?missing preset}; shift 2 ;;
        --keep) KEEP=1; shift ;;
        -h|--help)
            echo "Usage: scripts/agent-smoke.sh [--preset NAME] [--keep]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

RWCLI="$ROOT/build/$PRESET/rwcli"
if [ ! -x "$RWCLI" ]; then
    echo "rwcli not found at $RWCLI. Build the rwcli target first." >&2
    exit 1
fi

TMP=$(mktemp -d "${TMPDIR:-/tmp}/rwcli-smoke.XXXXXX")
cleanup() {
    if [ "$KEEP" -eq 0 ]; then
        rm -rf "$TMP"
    else
        echo "Kept smoke project: $TMP"
    fi
}
trap cleanup EXIT

cp -R "$ROOT/templates/Simple2D/." "$TMP/"

echo "Agent smoke project: $TMP"
"$RWCLI" doctor "$TMP" --json >/dev/null
"$RWCLI" lsp "$TMP" --json >/dev/null

echo "scaffold: templates/new/new-level" >&2
"$RWCLI" templates --json > "$TMP/templates.json"
grep -q 'Empty2D' "$TMP/templates.json"
SCAFFOLD="$TMP/ScaffoldGame"
"$RWCLI" new "$SCAFFOLD" --template Empty2D --json > "$TMP/new.json"
grep -q '"ok":true' "$TMP/new.json"
"$RWCLI" new-level "$SCAFFOLD" ExtraLevel --json > "$TMP/new-level.json"
grep -q '"ok":true' "$TMP/new-level.json"
grep -q '"registrationAdded":true' "$TMP/new-level.json"
[ -f "$SCAFFOLD/GameLogic/ExtraLevel.h" ]
"$RWCLI" build "$SCAFFOLD" --json > "$TMP/scaffold-build.json"
grep -q '"ok":true' "$TMP/scaffold-build.json"

WRAPPER="$TMP/.raywaves/bin/rwcli"
[ -x "$WRAPPER" ] || { echo "rwcli wrapper missing" >&2; exit 1; }

"$WRAPPER" build "$TMP" --json > "$TMP/build.json"
grep -q '"ok":true' "$TMP/build.json"

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "No DISPLAY/WAYLAND_DISPLAY. Run under xvfb-run for headless CI." >&2
    exit 1
fi

RUN_JSON="$TMP/run.json"
"$WRAPPER" run "$TMP" --frames 60 --screenshot shot.png --state state.json --json > "$RUN_JSON"
grep -q '"ok":true' "$RUN_JSON"
grep -q '"nonEmpty":true' "$RUN_JSON"
[ -f "$TMP/state.json" ]

printf '0,KEY_RIGHT,down\n30,KEY_RIGHT,up\n' > "$TMP/input.txt"
"$WRAPPER" run "$TMP" --frames 30 --input input.txt --json > "$TMP/input-run.json"
grep -q '"ok":true' "$TMP/input-run.json"

"$WRAPPER" scene add "$TMP" --map SmokeMap --name Spawn --type spawn --json > "$TMP/scene.json"
grep -q '"ok":true' "$TMP/scene.json"
grep -q 'type=spawn' "$TMP/Assets/SmokeMap.scene"

"$WRAPPER" validate "$TMP" --json > "$TMP/validate.json"
grep -q '"ok":true' "$TMP/validate.json"

if command -v clangd >/dev/null 2>&1; then
    "$WRAPPER" lsp "$TMP" --check --json > "$TMP/lsp.json"
    grep -q '"ok":true' "$TMP/lsp.json"
else
    echo "clangd not found; skipping LSP check" >&2
fi

echo "Agent smoke passed."
