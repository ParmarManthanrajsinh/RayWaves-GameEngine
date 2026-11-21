#!/usr/bin/env bash
# Shared bash helpers for Linux developer scripts.

raywaves_assert_linux_build_cache() {
    local build_dir=$1
    local cache="$build_dir/CMakeCache.txt"
    [ -f "$cache" ] || return 0

    local compiler
    compiler=$(sed -n 's/^CMAKE_CXX_COMPILER:STRING=//p' "$cache" | head -n 1)
    case "$compiler" in
        *zig-cxx*|*.bat|*.exe)
            echo "error: $(basename "$build_dir") was configured with a non-GCC/Windows toolchain." >&2
            echo "       Run scripts/clean.sh --preset $(basename "$build_dir") and reconfigure." >&2
            return 1
            ;;
    esac
    return 0
}
