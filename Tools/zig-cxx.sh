#!/usr/bin/env sh
# zig-cxx.sh - C++ compiler driver used by the Linux CMake presets.
# Prefers the toolchain fetched into Tools/zig, then falls back to PATH.
set -eu

TOOLS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ -x "$TOOLS_DIR/zig/zig" ]; then
    exec "$TOOLS_DIR/zig/zig" c++ "$@"
fi

if command -v zig >/dev/null 2>&1; then
    exec zig c++ "$@"
fi

echo "zig-cxx.sh: Zig not found. Run Tools/setup_tools.sh or install zig on PATH." >&2
exit 1
