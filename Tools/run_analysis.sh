#!/bin/sh
# run_analysis.sh - clang-tidy performance analysis + clang-format checks
# Usage:  run_analysis.sh [command] [options]
#
# Commands:
#   tidy          Run clang-tidy (all checks from .clang-tidy)
#   tidy-perf     Run only performance-* checks
#   tidy-fix      Run clang-tidy and apply fixes
#   format        Format all source files in-place with clang-format
#   format-check  Check formatting (exit 1 if any file is unformatted)
#   build-tidy    Build with clang-tidy enabled via CMake
#   report        Run tidy + dump summary grouped by diagnostic
#   all           Full pipeline: format-check -> tidy -> build
#   help          Show this help
#
# Options:
#   --preset <name>    CMake preset (default: linux-debug)
#   --jobs <n>         Parallel clang-tidy jobs (default: 0 = all cores)
#   --source <dir>     Limit to specific source dir (e.g. Engine, Editor)
set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

# --- defaults ---------------------------------------------------------------
PRESET="linux-debug"
JOBS="0"
SOURCE_FILTER=""
CMD=""

# --- parse args -------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --preset)   PRESET="$2";          shift 2 ;;
        --jobs)     JOBS="$2";            shift 2 ;;
        --source)   SOURCE_FILTER="$2";   shift 2 ;;
        help|tidy|tidy-perf|tidy-fix|format|format-check|build-tidy|report|all)
            CMD="$1"; shift ;;
        -h|--help)  CMD="help"; shift ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done
[ -z "$CMD" ] && CMD="help"

# --- locate tools (PATH first, then distro LLVM dirs) -----------------------
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

CLANG_TIDY=$(find_tool clang-tidy)     || CLANG_TIDY=""
CLANG_FORMAT=$(find_tool clang-format) || CLANG_FORMAT=""

# --- build dir setup --------------------------------------------------------
BUILD_DIR="$PROJECT_ROOT/build/$PRESET"
COMPILE_COMMANDS="$BUILD_DIR/compile_commands.json"

fail()
{
    echo "[FAIL] $*" >&2
    exit 1
}

ensure_compile_commands()
{
    if [ -f "$COMPILE_COMMANDS" ]; then
        return 0
    fi
    echo "== Generating compile_commands.json via cmake preset $PRESET ..."
    cmake --preset "$PRESET" || fail "cmake configure failed"
    [ -f "$COMPILE_COMMANDS" ] || fail "compile_commands.json not generated"
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

require_tidy()
{
    [ -n "$CLANG_TIDY" ] ||
        fail "clang-tidy not found. Install LLVM or add to PATH."
}

require_format()
{
    [ -n "$CLANG_FORMAT" ] ||
        fail "clang-format not found. Install LLVM or add to PATH."
}

run_tidy()
{
    ensure_compile_commands
    SOURCE_LIST=$(mktemp)
    trap 'rm -f "$SOURCE_LIST"' EXIT
    collect_sources > "$SOURCE_LIST"
    [ -s "$SOURCE_LIST" ] || fail "no source files found"

    TIDY_LOG="$BUILD_DIR/tidy_report.txt"
    echo "== Running clang-tidy on sources listed in $SOURCE_LIST ..."
    echo "== Log: $TIDY_LOG"

    : > "$TIDY_LOG"
    TIDY_EXIT=0
    while IFS= read -r f; do
        # shellcheck disable=SC2086
        "$CLANG_TIDY" -p "$BUILD_DIR" -quiet ${TIDY_CHECKS:-} "$f" \
            >> "$TIDY_LOG" 2>&1 || TIDY_EXIT=$?
    done < "$SOURCE_LIST"

    if [ "${REPORT_MODE:-0}" = "1" ]; then
        echo
        echo "== Grouped by check =="
        if grep -E "warning:" "$TIDY_LOG" >/dev/null 2>&1; then
            grep -E "warning:" "$TIDY_LOG" | sort
        else
            echo "No warnings found. Clean!"
        fi
        echo
        echo "Full log: $TIDY_LOG"
    fi

    return "$TIDY_EXIT"
}

case "$CMD" in
    help)
        echo
        echo "== RayWaves Analysis Tool =="
        echo
        echo "Usage:  run_analysis.sh [command] [options]"
        echo
        echo "Commands:"
        echo "  tidy           Run clang-tidy (all checks from .clang-tidy)"
        echo "  tidy-perf      Run only performance-* checks"
        echo "  tidy-fix       Run clang-tidy and apply fixes"
        echo "  format         Format all source files in-place"
        echo "  format-check   Check formatting (exit 1 if any unformatted)"
        echo "  build-tidy     Configure and build with clang-tidy enabled"
        echo "  report         Run tidy + dump summary grouped by diagnostic"
        echo "  all            Full pipeline: format-check -> tidy -> build"
        echo
        echo "Options:"
        echo "  --preset <name>     CMake preset (default: linux-debug)"
        echo "  --jobs <n>          Parallel jobs (default: 0 = all cores)"
        echo "  --source <dir>      Limit to specific source dir"
        echo
        if [ -n "$CLANG_TIDY" ]; then
            echo "  clang-tidy : found at $CLANG_TIDY"
        else
            echo "  clang-tidy : NOT FOUND"
        fi
        if [ -n "$CLANG_FORMAT" ]; then
            echo "  clang-format: found at $CLANG_FORMAT"
        else
            echo "  clang-format: NOT FOUND"
        fi
        echo "  compile_commands: $COMPILE_COMMANDS"
        if [ -f "$COMPILE_COMMANDS" ]; then
            echo "                  (exists)"
        else
            echo "                  (missing - will auto-generate)"
        fi
        echo
        ;;

    tidy)       require_tidy; TIDY_CHECKS="";          run_tidy ;;
    tidy-perf)  require_tidy; TIDY_CHECKS="--checks=performance-*"; run_tidy ;;
    tidy-fix)   require_tidy; TIDY_CHECKS="--fix";     run_tidy ;;
    report)     require_tidy; TIDY_CHECKS=""; REPORT_MODE=1; run_tidy ;;

    format)
        require_format
        echo "== Formatting all source files ..."
        collect_sources | while IFS= read -r f; do
            case "$f" in
                *.h) "$CLANG_FORMAT" -i -style=file "$f" ;;
                *)   "$CLANG_FORMAT" -i -style=file "$f" ;;
            esac
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

    build-tidy)
        echo "== Configuring with clang-tidy enabled ..."
        cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DENABLE_CLANG_TIDY=ON ||
            exit $?
        echo "== Building ..."
        if [ "$JOBS" -gt 0 ] 2>/dev/null; then
            cmake --build "$BUILD_DIR" -j "$JOBS"
        else
            cmake --build "$BUILD_DIR" -j "$(nproc 2>/dev/null || echo 4)"
        fi
        exit $?
        ;;

    all)
        ensure_compile_commands
        echo
        echo "== PHASE 1: Format check =="
        "$SCRIPT_DIR/run_analysis.sh" format-check --preset "$PRESET" ||
            exit $?
        echo
        echo "== PHASE 2: clang-tidy =="
        "$SCRIPT_DIR/run_analysis.sh" tidy --preset "$PRESET" || exit $?
        echo
        echo "== PHASE 3: Build =="
        if [ "$JOBS" -gt 0 ] 2>/dev/null; then
            cmake --build "$BUILD_DIR" -j "$JOBS"
        else
            cmake --build "$BUILD_DIR" -j "$(nproc 2>/dev/null || echo 4)"
        fi
        exit $?
        ;;

    *)
        echo "Unknown command: $CMD" >&2
        exit 1
        ;;
esac
