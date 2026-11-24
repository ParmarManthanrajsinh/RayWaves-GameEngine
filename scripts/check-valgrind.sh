#!/usr/bin/env bash
# Configure, build, and run a test suite under Valgrind memcheck.
#
# Linux only: Valgrind does not support the Windows toolchain, and Windows
# builds force -fno-sanitize=all in cmake/Platform.cmake.
#
# Usage:
#   chmod +x scripts/check-valgrind.sh   # once per checkout
#   ./scripts/check-valgrind.sh                    # unit suite (default)
#   ./scripts/check-valgrind.sh --suite all        # every suite, slow
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/lib.sh
. "$ROOT/scripts/lib.sh"

case "$(uname -s)" in
    Linux) ;;
    *)
        echo "error: Valgrind checks are Linux-only; Windows builds disable sanitizers." >&2
        exit 1
        ;;
esac

if ! command -v valgrind >/dev/null 2>&1; then
    echo "error: valgrind not found. Install it first, e.g. sudo apt install valgrind" >&2
    exit 1
fi

SUITE="unit"
while [ $# -gt 0 ]; do
    case "$1" in
        --suite|-s) SUITE=${2:?missing suite}; shift 2 ;;
        -h|--help)
            echo "Usage: scripts/check-valgrind.sh [--suite unit|integration|smoke|all]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

case "$SUITE" in
    unit|integration|smoke|all) ;;
    *) echo "Invalid suite: $SUITE" >&2; exit 2 ;;
esac

# Dedicated build tree so a sanitizer-configured zig-debug cannot mix with
# Valgrind in the same objects. Mirrors build/asan in check-sanitize.sh.
BUILD_DIR="$ROOT/build/valgrind"
raywaves_assert_linux_build_cache "$BUILD_DIR"
cd "$ROOT"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DRAYWAVES_TOOLCHAIN=system \
        -DRAYWAVES_BUILD_BENCHMARKS=OFF
fi

# Definite leaks fail the run; still-reachable and possible leaks from
# raylib/X11/GL teardown are reported but do not fail it. --trace-children=no
# keeps cmake/ninja subprocesses spawned by the suites out of Valgrind.
VALGRIND_OPTS=(
    --leak-check=full
    --show-leak-kinds=definite
    --errors-for-leak-kinds=definite
    --track-origins=yes
    --error-exitcode=99
    --trace-children=no
)

run_valgrind() {
    if command -v xvfb-run >/dev/null 2>&1; then
        LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}" \
            xvfb-run -a valgrind "${VALGRIND_OPTS[@]}" "$@"
    else
        valgrind "${VALGRIND_OPTS[@]}" "$@"
    fi
}

TARGETS=""
RUNS=()
case "$SUITE" in
    unit)
        TARGETS="raywaves_tests_unit"
        RUNS=("$BUILD_DIR/raywaves_tests_unit")
        ;;
    integration)
        TARGETS="raywaves_tests_integration"
        RUNS=("$BUILD_DIR/raywaves_tests_integration")
        ;;
    smoke)
        TARGETS="raywaves_smoke"
        RUNS=("$BUILD_DIR/raywaves_smoke --iterations 3")
        ;;
    all)
        TARGETS="raywaves_tests_unit raywaves_tests_integration raywaves_smoke"
        RUNS=(
            "$BUILD_DIR/raywaves_tests_unit"
            "$BUILD_DIR/raywaves_tests_integration"
            "$BUILD_DIR/raywaves_smoke --iterations 3"
        )
        ;;
esac

# shellcheck disable=SC2086
cmake --build "$BUILD_DIR" --target $TARGETS

# Each RUNS entry is a whole command line, word-split so the smoke binary
# keeps its --iterations argument. Run from the build dir: the suites resolve
# rwcli and GameLogic through the current path, exactly as CTest does via the
# WORKING_DIRECTORY test property.
for run in "${RUNS[@]}"; do
    # shellcheck disable=SC2086
    (cd "$BUILD_DIR" && run_valgrind $run)
done
