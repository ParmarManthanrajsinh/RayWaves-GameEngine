#!/usr/bin/env bash
# Configure and build a preset. Mirrors scripts/build.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/lib.sh
. "$ROOT/scripts/lib.sh"
PRESET="zig-release"
TARGET=""

while [ $# -gt 0 ]; do
    case "$1" in
        --preset|-p) PRESET=${2:?missing preset}; shift 2 ;;
        --target|-t) TARGET=${2:?missing target}; shift 2 ;;
        -h|--help)
            echo "Usage: scripts/build.sh [--preset NAME] [--target NAME]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

BUILD_DIR="$ROOT/build/$PRESET"
raywaves_assert_linux_build_cache "$BUILD_DIR"
cd "$ROOT"

if [ ! -d "$BUILD_DIR" ]; then
    cmake --preset "$PRESET"
fi

if [ -n "$TARGET" ]; then
    cmake --build "$BUILD_DIR" --target "$TARGET"
else
    cmake --build "$BUILD_DIR"
fi
