#!/usr/bin/env bash
# Markdown documentation checks. Mirrors scripts/check-docs.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [ $# -gt 0 ] && [ -n "${1:-}" ]; then
    ROOT=$1
fi

errors=0

stale_tokens=(
    "DemoGame"
    "build_gamelogic.bat"
    "dist_CMakeLists.txt"
    "create_distribution.bat"
    "run_all.bat"
    "run_tests.bat"
    "run_smoketest.bat"
    "Tests/"
    "Distribution/"
    "Documentation/"
)

while IFS= read -r -d '' file; do
    relative=${file#"$ROOT"/}

    if [[ "$relative" != docs/adr/* ]]; then
        for token in "${stale_tokens[@]}"; do
            if grep -qF -- "$token" "$file"; then
                echo "$relative: stale token '$token'" >&2
                errors=$((errors + 1))
            fi
        done
    fi

    fences=$(grep -o '```' "$file" | wc -l || true)
    if [ $((fences % 2)) -ne 0 ]; then
        echo "$relative: unbalanced code fence" >&2
        errors=$((errors + 1))
    fi

    dir=$(dirname "$file")
    while IFS= read -r match; do
        [ -n "$match" ] || continue
        target=${match#](}
        target=${target%)}
        target=${target#<}
        target=${target%>}
        case "$target" in
            http://*|https://*|mailto:*|"#"*) continue ;;
        esac
        target=${target%%#*}
        [ -n "$target" ] || continue
        if [ ! -e "$dir/$target" ]; then
            echo "$relative: broken link -> $target" >&2
            errors=$((errors + 1))
        fi
    done < <(grep -oE '\]\([^)]+\)' "$file" || true)
done < <(find "$ROOT" \
    \( -path "$ROOT/build" -o -path "$ROOT/dist" -o -path "$ROOT/.git" \
       -o -path "$ROOT/third_party" -o -path "$ROOT/.agents" -o -path "$ROOT/Tools" \) -prune -o \
    -name '*.md' -type f -print0)

if [ "$errors" -gt 0 ]; then
    echo "Documentation check failed ($errors issues)." >&2
    exit 1
fi

count=$(find "$ROOT" \
    \( -path "$ROOT/build" -o -path "$ROOT/dist" -o -path "$ROOT/.git" \
       -o -path "$ROOT/third_party" -o -path "$ROOT/.agents" -o -path "$ROOT/Tools" \) -prune -o \
    -name '*.md' -type f -print | wc -l)
echo "Documentation check passed ($count files)."
