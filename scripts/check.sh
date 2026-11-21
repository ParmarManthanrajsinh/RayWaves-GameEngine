#!/usr/bin/env bash
# Format check + documentation check + full test suite. Mirrors scripts/check.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/lib.sh
. "$ROOT/scripts/lib.sh"
PRESET="zig-debug"

while [ $# -gt 0 ]; do
    case "$1" in
        --preset|-p) PRESET=${2:?missing preset}; shift 2 ;;
        -h|--help)
            echo "Usage: scripts/check.sh [--preset NAME]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

raywaves_assert_linux_build_cache "$ROOT/build/$PRESET"
cd "$ROOT"

if command -v clang-format >/dev/null 2>&1; then
    echo "Checking formatting..."
    # Keep the same gate as scripts/check.ps1 / Tools/run_analysis.bat:
    # top-level Engine/Editor/Runtime sources, editor panels/terminal, and the
    # bundled template GameLogic. Vendored code and raygui are excluded.
    mapfile -t files < <(
        {
            find Engine Editor Runtime -maxdepth 1 \( -name '*.cpp' -o -name '*.h' \) -type f ! -name 'raygui.h'
            find Editor/Panels Editor/terminal -maxdepth 1 \( -name '*.cpp' -o -name '*.h' \) -type f
            find tests -maxdepth 1 \( -name '*.cpp' -o -name '*.h' \) -type f
            find templates/SlimeQuest/GameLogic templates/Platformer2D/GameLogic \
                -maxdepth 1 \( -name '*.cpp' -o -name '*.h' \) -type f
        } | sort
    )
    clang-format --dry-run --Werror --style=file "${files[@]}"
else
    echo "clang-format not found; skipping format check" >&2
fi

"$ROOT/scripts/check-docs.sh" "$ROOT"
"$ROOT/scripts/test.sh" --preset "$PRESET" --suite all
