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
    # Walk the whole tree, not just the first level: platform backends and
    # tests live in subdirectories. Vendored UI code and raygui are excluded.
    mapfile -t files < <(
        {
            find Engine Editor Runtime Cli \
                \( -path '*/imgui/*' -o -path '*/rlImGui/*' -o -path '*/tinyfiledialogs/*' \
                   -o -name 'raygui.h' \) -prune -o \
                \( -name '*.cpp' -o -name '*.h' \) -type f -print
            find tests \( -name '*.cpp' -o -name '*.h' \) -type f -print
            find templates/SlimeQuest/GameLogic templates/Platformer2D/GameLogic \
                templates/ThirdPerson3D/GameLogic \( -name '*.cpp' -o -name '*.h' \) -type f -print
        } | sort
    )
    clang-format --dry-run --Werror --style=file "${files[@]}"
else
    echo "clang-format not found; skipping format check" >&2
fi

"$ROOT/scripts/check-docs.sh" "$ROOT"
"$ROOT/scripts/test.sh" --preset "$PRESET" --suite all
