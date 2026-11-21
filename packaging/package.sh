#!/usr/bin/env bash
# Build the RayWaves engine distribution on Linux.
# Mirrors packaging/package.ps1.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
CONFIG="Release"
OUTPUT_DIR="dist"
INCLUDE_COMPILER=0
ARCHIVE=0
VERSION=""

while [ $# -gt 0 ]; do
    case "$1" in
        --config|-c) CONFIG=${2:?missing config}; shift 2 ;;
        --output-dir|-o) OUTPUT_DIR=${2:?missing output dir}; shift 2 ;;
        --include-compiler) INCLUDE_COMPILER=1; shift ;;
        --archive|--tar|--zip) ARCHIVE=1; shift ;;
        --version|-v) VERSION=${2:?missing version}; shift 2 ;;
        -h|--help)
            echo "Usage: scripts/package.sh [--config Release|Debug] [--output-dir DIR] [--include-compiler] [--archive] [--version X.Y.Z]"
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

case "$CONFIG" in
    Release|Debug) ;;
    *) echo "Invalid config: $CONFIG" >&2; exit 2 ;;
esac

# Never let a typo turn rm -rf "$DIST" into a repository/home wipe.
case "$OUTPUT_DIR" in
    ""|"/"|"."|"..")
        echo "error: refusing unsafe output directory: '$OUTPUT_DIR'" >&2
        exit 2
        ;;
    *".."*)
        echo "error: output directory must not contain '..': '$OUTPUT_DIR'" >&2
        exit 2
        ;;
esac

case "$(uname -m)" in
    x86_64|amd64) ARCH_LABEL="x86_64" ;;
    *) echo "error: Linux packages are x86_64 only (found $(uname -m))." >&2; exit 1 ;;
esac

if [ -z "$VERSION" ]; then
    VERSION=$(sed -n 's/.*project(RayWaves VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -n 1)
    VERSION=${VERSION:-0.6.0}
fi

PRESET="zig-$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="$ROOT/build/$PRESET"
if [[ "$OUTPUT_DIR" = /* ]]; then
    DIST="$OUTPUT_DIR"
else
    DIST="$ROOT/$OUTPUT_DIR"
fi

case "$DIST" in
    "/"|"$ROOT"|"$ROOT/"|"${HOME:-/nonexistent}"|"${HOME:-/nonexistent}/")
        echo "error: refusing to package into '$DIST'" >&2
        exit 2
        ;;
esac

# The output directory is deleted with rm -rf. Absolute paths must stay in a
# user-owned scratch area, the repo, or $HOME; a typo like /usr/local must not
# become a filesystem-wide delete.
if [[ "$DIST" = /* ]]; then
    case "$DIST" in
        "$ROOT"/*|"${HOME:-/nonexistent}"/*|/tmp/*) ;;
        *)
            echo "error: absolute output directory must be under \$HOME, /tmp, or the repo: '$DIST'" >&2
            exit 2
            ;;
    esac
fi

echo "Packaging RayWaves $VERSION ($CONFIG)"
echo "  build:  $BUILD_DIR"
echo "  output: $DIST"

# The distribution must include the project toolchain (CMake + Ninja) so a
# user does not need them globally.
if [ ! -x "$ROOT/Tools/cmake/bin/cmake" ] || [ ! -x "$ROOT/Tools/ninja/ninja" ]; then
    "$ROOT/Tools/setup_tools.sh" --skip-zig --skip-rcedit
fi

if [ -f "$BUILD_DIR/CMakeCache.txt" ] && grep -q '^CMAKE_CXX_COMPILER:STRING=.*zig' "$BUILD_DIR/CMakeCache.txt"; then
    echo "error: $BUILD_DIR was configured with Zig. Run scripts/clean.sh --preset $PRESET." >&2
    exit 1
fi

if [ ! -d "$BUILD_DIR" ]; then
    cmake --preset "$PRESET"
fi
cmake --build "$BUILD_DIR" --target game main rwcli

GAME_EXE="$BUILD_DIR/game"
EDITOR_EXE="$BUILD_DIR/RayWaves"
CLI_EXE="$BUILD_DIR/rwcli"
RAYLIB_STAGED="$BUILD_DIR/raylib"

require_file() {
    if [ ! -e "$1" ]; then
        echo "Missing $2 at $1" >&2
        exit 1
    fi
}

require_file "$GAME_EXE" "runtime executable"
require_file "$EDITOR_EXE" "editor executable"
require_file "$CLI_EXE" "rwcli executable"
require_file "$RAYLIB_STAGED/include/raylib.h" "raylib headers"
require_file "$RAYLIB_STAGED/lib/libraylib.so" "raylib library"

rm -rf "$DIST"
mkdir -p "$DIST/Core/raylib/include" "$DIST/Core/raylib/lib" "$DIST/Core/raylib/bin" "$DIST/docs"

copy_file() {
    require_file "$1" "$1"
    mkdir -p "$(dirname "$2")"
    cp -f "$1" "$2"
}

copy_dir() {
    require_file "$1" "$1"
    mkdir -p "$2"
    cp -R "$1/." "$2/"
}

# Root executables and runtime library.
copy_file "$GAME_EXE" "$DIST/Core/runtime"
copy_file "$EDITOR_EXE" "$DIST/RayWaves"
copy_file "$CLI_EXE" "$DIST/rwcli"

# raylib's SONAME is libraylib.so.600. Keep the linker name and the SONAME
# file; the extra versioned copy is identical and only bloats the archive.
copy_file "$RAYLIB_STAGED/lib/libraylib.so" "$DIST/libraylib.so"
if [ -f "$RAYLIB_STAGED/lib/libraylib.so.600" ]; then
    copy_file "$RAYLIB_STAGED/lib/libraylib.so.600" "$DIST/libraylib.so.600"
fi

# The runtime lives in Core/, so it needs its own copy of raylib.
if [ -f "$RAYLIB_STAGED/lib/libraylib.so.600" ]; then
    copy_file "$RAYLIB_STAGED/lib/libraylib.so.600" "$DIST/Core/libraylib.so.600"
fi
copy_file "$RAYLIB_STAGED/lib/libraylib.so" "$DIST/Core/libraylib.so"
cp -f "$RAYLIB_STAGED"/include/*.h "$DIST/Core/raylib/include/"
copy_file "$RAYLIB_STAGED/lib/libraylib.so" "$DIST/Core/raylib/lib/libraylib.so"
if [ -f "$RAYLIB_STAGED/lib/libraylib.so.600" ]; then
    copy_file "$RAYLIB_STAGED/lib/libraylib.so.600" "$DIST/Core/raylib/lib/libraylib.so.600"
fi

# Engine sources and headers used by generated project builds.
mkdir -p "$DIST/Core/Engine"
cp -f "$ROOT"/Engine/*.h "$DIST/Core/Engine/"
cp -f "$ROOT"/Engine/*.cpp "$DIST/Core/Engine/"

# Project generation templates.
cp -f "$ROOT"/packaging/Project.*.in "$DIST/Core/"
cp -f "$ROOT"/packaging/Project.vscode.*.json.in "$DIST/Core/"
cp -f "$ROOT"/.clang-format "$DIST/Core/" 2>/dev/null || true

copy_dir "$ROOT/templates" "$DIST/templates"
copy_dir "$ROOT/EngineContent" "$DIST/Core/EngineContent"
copy_dir "$ROOT/docs" "$DIST/docs"
copy_file "$ROOT/packaging/config.ini" "$DIST/config.ini"
copy_file "$ROOT/LICENSE" "$DIST/LICENSE"
copy_file "$ROOT/THIRD_PARTY_NOTICES.md" "$DIST/THIRD_PARTY_NOTICES.md"
printf '%s\n' "$VERSION" > "$DIST/VERSION"

# Toolchain wrappers and the fetch scripts. The bundled compiler is optional.
mkdir -p "$DIST/Core/Tools"
cp -f "$ROOT"/Tools/setup_tools.sh "$ROOT"/Tools/toolchain.ini "$DIST/Core/Tools/"
if [ -d "$ROOT/Tools/ninja" ]; then
    copy_dir "$ROOT/Tools/ninja" "$DIST/Core/Tools/ninja"
fi
# Bundle only the CMake pieces needed to configure and build a project.
# Copying the full tree adds ~200 MB of documentation and GUI tools.
if [ -d "$ROOT/Tools/cmake" ]; then
    mkdir -p "$DIST/Core/Tools/cmake/bin" "$DIST/Core/Tools/cmake/share"
    # Project builds need cmake, not ctest/cpack or the GUI tools.
    for tool in cmake; do
        if [ -x "$ROOT/Tools/cmake/bin/$tool" ]; then
            copy_file "$ROOT/Tools/cmake/bin/$tool" "$DIST/Core/Tools/cmake/bin/$tool"
        fi
    done
    for cmake_share in "$ROOT"/Tools/cmake/share/cmake-*; do
        [ -d "$cmake_share" ] || continue
        share_name=$(basename "$cmake_share")
        for subdir in Modules Templates Licenses; do
            if [ -d "$cmake_share/$subdir" ]; then
                copy_dir "$cmake_share/$subdir" "$DIST/Core/Tools/cmake/share/$share_name/$subdir"
            fi
        done
    done
fi
if [ "$INCLUDE_COMPILER" -eq 1 ]; then
    echo "warning: Linux packages use the system GCC/G++; --include-compiler has no effect." >&2
fi

# Remove build artifacts accidentally copied from templates.
find "$DIST/templates" -type f \( -name '*.so' -o -name '*.o' -o -name '*.a' \) -delete 2>/dev/null || true
find "$DIST/templates" -type d -name '.raywaves' -prune -exec rm -rf {} + 2>/dev/null || true

chmod +x "$DIST/RayWaves" "$DIST/rwcli" "$DIST/Core/runtime"
if [ -f "$DIST/Core/Tools/ninja/ninja" ]; then chmod +x "$DIST/Core/Tools/ninja/ninja"; fi
if [ -d "$DIST/Core/Tools/cmake/bin" ]; then chmod +x "$DIST/Core/Tools/cmake/bin/"*; fi
find "$DIST/Core/Tools" -name '*.sh' -type f -exec chmod +x {} +

# Release packages do not need debug symbols. Keep Debug packages untouched.
if [ "$CONFIG" = "Release" ] && command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$DIST/RayWaves" "$DIST/rwcli" "$DIST/Core/runtime" 2>/dev/null || true
fi

# Report the actual runtime ABI floor. Release artifacts are built on an
# older LTS in CI; a local bleeding-edge distro will legitimately need a
# newer glibc, so enforce only when the caller asks for it.
GLIBC_BASELINE=${RAYWAVES_GLIBC_BASELINE:-2.39}
GLIBCXX_BASELINE=${RAYWAVES_GLIBCXX_BASELINE:-3.4.32}
version_gt() {
    [ "$1" != "$2" ] && [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" = "$1" ]
}
check_binary_abi() {
    local binary=$1
    [ -f "$binary" ] || return 0
    command -v objdump >/dev/null 2>&1 || return 0

    local max_glibc max_glibcxx
    max_glibc=$(objdump -T "$binary" 2>/dev/null | grep -o 'GLIBC_[0-9.]*' | sed 's/^GLIBC_//' | sort -Vu | tail -n 1 || true)
    max_glibcxx=$(objdump -T "$binary" 2>/dev/null | grep -o 'GLIBCXX_[0-9.]*' | sed 's/^GLIBCXX_//' | sort -Vu | tail -n 1 || true)
    [ -n "$max_glibc" ] && echo "  $(basename "$binary"): glibc >= $max_glibc, libstdc++ >= ${max_glibcxx:-n/a}"
    if version_gt "$max_glibc" "$GLIBC_BASELINE" || version_gt "$max_glibcxx" "$GLIBCXX_BASELINE"; then
        if [ "${RAYWAVES_ENFORCE_GLIBC:-0}" = "1" ]; then
            echo "error: $(basename "$binary") requires a newer ABI than the ${GLIBC_BASELINE}/${GLIBCXX_BASELINE} baseline." >&2
            echo "       Build release artifacts in the pinned older container." >&2
            exit 1
        fi
        echo "warning: $(basename "$binary") exceeds the documented ${GLIBC_BASELINE}/${GLIBCXX_BASELINE} baseline." >&2
    fi
}
echo "Runtime ABI requirements:"
check_binary_abi "$DIST/RayWaves"
check_binary_abi "$DIST/rwcli"
check_binary_abi "$DIST/Core/runtime"
check_binary_abi "$DIST/libraylib.so.600"

# Validation.
required=(
    "RayWaves"
    "rwcli"
    "libraylib.so"
    "libraylib.so.600"
    "config.ini"
    "VERSION"
    "LICENSE"
    "THIRD_PARTY_NOTICES.md"
    "docs/licenses/raylib.txt"
    "Core/runtime"
    "Core/Project.CMakeLists.in"
    "Core/Project.AGENTS.md.in"
    "Core/Project.SKILL.md.in"
    "Core/Project.rwcli-reference.md.in"
    "Core/Project.clangd.in"
    "Core/raylib/include/raylib.h"
    "Core/raylib/lib/libraylib.so"
    "docs/README.md"
)
for relative in "${required[@]}"; do
    require_file "$DIST/$relative" "package file $relative"
done

if [ "$ARCHIVE" -eq 1 ]; then
    ARCHIVE_NAME="RayWaves-$VERSION-linux-$ARCH_LABEL"
    if [ "$CONFIG" != "Release" ]; then
        ARCHIVE_NAME="$ARCHIVE_NAME-$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"
    fi
    ARCHIVE_PATH="$ROOT/dist/$ARCHIVE_NAME.tar.gz"
    mkdir -p "$(dirname "$ARCHIVE_PATH")"
    rm -f "$ARCHIVE_PATH"
    # Build the archive outside DIST: writing it inside the tree while tar
    # reads that tree triggers "file changed as we read it".
    TMP_ARCHIVE=$(mktemp "${TMPDIR:-/tmp}/raywaves-package.XXXXXX.tar.gz")
    tar -czf "$TMP_ARCHIVE" -C "$DIST" .
    mv -f "$TMP_ARCHIVE" "$ARCHIVE_PATH"
    chmod 644 "$ARCHIVE_PATH"
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$(dirname "$ARCHIVE_PATH")" && sha256sum "$(basename "$ARCHIVE_PATH")" > "$(basename "$ARCHIVE_PATH").sha256")
    fi
    echo "Archive: $ARCHIVE_PATH"
fi

echo "Distribution ready: $DIST"
