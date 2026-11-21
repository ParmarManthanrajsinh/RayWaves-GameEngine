#!/usr/bin/env bash
# Remove a CMake build directory. Mirrors scripts/clean.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
PRESET=""

while [ $# -gt 0 ]; do
    case "$1" in
        --preset|-p) PRESET=${2:?missing preset}; shift 2 ;;
        --all) PRESET=""; shift ;;
        -h|--help)
            echo "Usage: scripts/clean.sh [--preset NAME|--all]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

if [ -n "$PRESET" ]; then
    rm -rf "$ROOT/build/$PRESET"
    echo "Removed build/$PRESET"
else
    rm -rf "$ROOT/build"
    echo "Removed build/"
fi
