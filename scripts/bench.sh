#!/usr/bin/env bash
# Build and run raywaves_benchmarks. Mirrors scripts/bench.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
PRESET="zig-release"

while [ $# -gt 0 ]; do
    case "$1" in
        --preset|-p) PRESET=${2:?missing preset}; shift 2 ;;
        --) shift; break ;;
        *) break ;;
    esac
done

BUILD_DIR="$ROOT/build/$PRESET"
cd "$ROOT"

if [ ! -d "$BUILD_DIR" ]; then
    cmake --preset "$PRESET"
fi

cmake --build "$BUILD_DIR" --target raywaves_benchmarks
"$BUILD_DIR/raywaves_benchmarks" "$@"
