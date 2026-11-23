#!/usr/bin/env bash
# Configure, build, and run the unit/integration suites under AddressSanitizer
# and UndefinedBehaviorSanitizer. Mirrors the Linux "sanitize" CI job.
#
# Linux only: Windows builds force -fno-sanitize=all in cmake/Platform.cmake.
#
# Usage:
#   chmod +x scripts/check-sanitize.sh   # once per checkout
#   ./scripts/check-sanitize.sh
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/lib.sh
. "$ROOT/scripts/lib.sh"

case "$(uname -s)" in
    Linux) ;;
    *)
        echo "error: sanitizers are Linux-only; Windows builds disable them." >&2
        exit 1
        ;;
esac

BUILD_DIR="$ROOT/build/asan"
raywaves_assert_linux_build_cache "$BUILD_DIR"
cd "$ROOT"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DRAYWAVES_TOOLCHAIN=system \
        -DRAYWAVES_BUILD_BENCHMARKS=OFF \
        -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
        -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined"
fi

cmake --build "$BUILD_DIR" --target raywaves_tests_unit raywaves_tests_integration

# Leak checking stays off: raylib/X11/GL teardown and forked children produce
# allocator noise that is not ours to fix. Override the variables to debug.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"

if command -v xvfb-run >/dev/null 2>&1; then
    LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}" \
        xvfb-run -a ctest --test-dir "$BUILD_DIR" -L "unit|integration" \
        --no-tests=error --output-on-failure
else
    ctest --test-dir "$BUILD_DIR" -L "unit|integration" \
        --no-tests=error --output-on-failure
fi
