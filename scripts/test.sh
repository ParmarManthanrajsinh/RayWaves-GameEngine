#!/usr/bin/env bash
# Build and run unit/integration/smoke suites. Mirrors scripts/test.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/lib.sh
. "$ROOT/scripts/lib.sh"
SUITE="all"
PRESET="zig-debug"

while [ $# -gt 0 ]; do
    case "$1" in
        --suite|-s) SUITE=${2:?missing suite}; shift 2 ;;
        --preset|-p) PRESET=${2:?missing preset}; shift 2 ;;
        -h|--help)
            echo "Usage: scripts/test.sh [--suite all|unit|integration|smoke] [--preset NAME]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

case "$SUITE" in
    all|unit|integration|smoke) ;;
    *) echo "Invalid suite: $SUITE" >&2; exit 2 ;;
esac

BUILD_DIR="$ROOT/build/$PRESET"
raywaves_assert_linux_build_cache "$BUILD_DIR"
cd "$ROOT"

if [ ! -d "$BUILD_DIR" ]; then
    cmake --preset "$PRESET"
fi

cmake --build "$BUILD_DIR" --target raywaves_tests_unit raywaves_tests_integration raywaves_smoke game main

LABEL="$SUITE"
if [ "$SUITE" = "all" ]; then
    LABEL="unit|integration|smoke"
fi

ctest --test-dir "$BUILD_DIR" -L "$LABEL" --output-on-failure
