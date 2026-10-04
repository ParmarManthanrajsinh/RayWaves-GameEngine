#!/bin/sh
# run_analysis.sh - clang-format driver for first-party sources.
# Usage:  run_analysis.sh [command] [options]
#
# Commands:
#   format         Format all source files in-place with clang-format
#   format-check   Check formatting (exit 1 if any file is unformatted)
#   help          Show this help
#
# Options:
#   --source <dir>     Limit to specific source dir (e.g. Engine, Editor)
set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

# --- defaults ---------------------------------------------------------------
SOURCE_FILTER=""
CMD=""

# --- parse args -------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --source)   SOURCE_FILTER="$2";   shift 2 ;;
        format|format-check|help)
            CMD="$1"; shift ;;
        -h|--help)  CMD="help"; shift ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done
[ -z "$CMD" ] && CMD="help"

# --- locate clang-format (PATH first, then distro LLVM dirs) ---------------
find_tool()
{
    tool="$1"
    if command -v "$tool" >/dev/null 2>&1; then
        command -v "$tool"
        return 0
    fi
    for d in /usr/lib/llvm-*/bin /usr/local/opt/llvm/bin /opt/llvm/bin; do
        if [ -x "$d/$tool" ]; then
            echo "$d/$tool"
            return 0
        fi
    done
    return 1
}

CLANG_FORMAT=$(find_tool clang-format) || CLANG_FORMAT=""

fail()
{
    echo "[FAIL] $*" >&2
    exit 1
}

require_format()
{
    [ -n "$CLANG_FORMAT" ] ||
        fail "clang-format not found. Install LLVM or add to PATH."
}

# Collect .cpp files matching the filter, skip vendored trees.
collect_sources()
{
    if [ -n "$SOURCE_FILTER" ]; then
        if [ -f "$PROJECT_ROOT/$SOURCE_FILTER" ]; then
            printf '%s\n' "$PROJECT_ROOT/$SOURCE_FILTER"
            return 0
        fi
        if [ -d "$PROJECT_ROOT/$SOURCE_FILTER" ]; then
            find "$PROJECT_ROOT/$SOURCE_FILTER" -name '*.cpp' -type f \
                ! -path '*/imgui/*' ! -path '*/tinyfiledialogs/*' \
                ! -path '*/rlImGui/*' ! -path '*/doctest/*'
            return 0
        fi
        fail "no source files found for --source $SOURCE_FILTER"
    fi

    find "$PROJECT_ROOT/Engine" "$PROJECT_ROOT/Editor" "$PROJECT_ROOT/Game" \
         "$PROJECT_ROOT/Tests" "$PROJECT_ROOT/Distribution/Templates" \
         -name '*.cpp' -type f \
         ! -path '*/imgui/*' ! -path '*/tinyfiledialogs/*' \
         ! -path '*/rlImGui/*' ! -path '*/doctest/*' 2>/dev/null
}

case "$CMD" in
    help)
        echo
        echo "== RayWaves Format Tool =="
        echo
        echo "Usage:  run_analysis.sh [command] [options]"
        echo
        echo "Commands:"
        echo "  format         Format all source files in-place"
        echo "  format-check   Check formatting (exit 1 if any unformatted)"
        echo
        echo "Options:"
        echo "  --source <dir>      Limit to specific source dir"
        echo
        if [ -n "$CLANG_FORMAT" ]; then
            echo "  clang-format: found at $CLANG_FORMAT"
        else
            echo "  clang-format: NOT FOUND"
        fi
        echo
        ;;

    format)
        require_format
        echo "== Formatting all source files ..."
        collect_sources | while IFS= read -r f; do
            "$CLANG_FORMAT" -i -style=file "$f"
        done
        echo "== Formatting done."
        ;;

    format-check)
        require_format
        echo "== Checking formatting ..."
        UNFORMATTED=0
        while IFS= read -r f; do
            if ! "$CLANG_FORMAT" -n -style=file -Werror "$f" >/dev/null 2>&1
            then
                echo "[UNFORMATTED] $f"
                UNFORMATTED=1
            fi
        done <<EOF
$(collect_sources)
EOF
        if [ "$UNFORMATTED" = "1" ]; then
            echo "== Format check FAILED. Run 'format' to fix."
            exit 1
        fi
        echo "== All files are properly formatted."
        ;;

    *)
        echo "Unknown command: $CMD" >&2
        exit 1
        ;;
esac
