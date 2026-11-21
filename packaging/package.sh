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

if [ -z "$VERSION" ]; then
    VERSION=$(sed -n 's/.*project(RayWaves VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -n 1)
    VERSION=${VERSION:-0.1.0}
fi

PRESET="zig-$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="$ROOT/build/$PRESET"
if [[ "$OUTPUT_DIR" = /* ]]; then
    DIST="$OUTPUT_DIR"
else
    DIST="$ROOT/$OUTPUT_DIR"
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
copy_file "$RAYLIB_STAGED/lib/libraylib.so" "$DIST/libraylib.so"
cp -f "$RAYLIB_STAGED"/lib/libraylib.so.* "$DIST/" 2>/dev/null || true

# The runtime lives in Core/, so it needs its own copy of raylib.
cp -f "$RAYLIB_STAGED"/lib/libraylib.so* "$DIST/Core/" 2>/dev/null || true
cp -f "$RAYLIB_STAGED"/include/*.h "$DIST/Core/raylib/include/"
cp -f "$RAYLIB_STAGED"/lib/libraylib.so* "$DIST/Core/raylib/lib/"
if [ -d "$RAYLIB_STAGED/bin" ]; then
    cp -f "$RAYLIB_STAGED"/bin/* "$DIST/Core/raylib/bin/" 2>/dev/null || true
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
printf '%s\n' "$VERSION" > "$DIST/VERSION"

# Toolchain wrappers and the fetch scripts. The bundled compiler is optional.
mkdir -p "$DIST/Core/Tools"
cp -f "$ROOT"/Tools/setup_tools.sh "$ROOT"/Tools/toolchain.ini "$DIST/Core/Tools/"
if [ -d "$ROOT/Tools/ninja" ]; then
    copy_dir "$ROOT/Tools/ninja" "$DIST/Core/Tools/ninja"
fi
if [ -d "$ROOT/Tools/cmake" ]; then
    copy_dir "$ROOT/Tools/cmake" "$DIST/Core/Tools/cmake"
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

# Validation.
required=(
    "RayWaves"
    "rwcli"
    "libraylib.so"
    "config.ini"
    "VERSION"
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
    ARCHIVE_PATH="$ROOT/dist/RayWaves-$VERSION-linux-x86_64.tar.gz"
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
