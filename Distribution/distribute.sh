#!/bin/sh
# Distribution Script for RayWaves Game Engine (Linux)
# Creates a distribution package with the editor, the hidden game runtime,
# and the development environment used to build GameLogic.so per project.
#
# Ports the former distribute.ps1. The Zig/rcedit/Ninja/CMake bundling block
# is intentionally gone: Linux builds against the system toolchain.
set -eu

BUILD_CONFIG="Release"
OUTPUT_DIR="dist"

while [ $# -gt 0 ]; do
    case "$1" in
        -BuildConfig|--build-config) BUILD_CONFIG="$2"; shift 2 ;;
        -OutputDir|--output-dir)     OUTPUT_DIR="$2";   shift 2 ;;
        -h|--help)
            echo "Usage: $0 [-BuildConfig Release|Debug] [-OutputDir DIR]"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

echo "Creating distribution package..."
echo "Build Config: $BUILD_CONFIG, Output Directory: $OUTPUT_DIR"

# Repo root = parent of this script's directory (works from any CWD).
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

# Ensure we have a build: presets own binaryDir (build/linux-<config>).
case "$(printf '%s' "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]')" in
    release) PRESET="linux-release"; BUILD_PATH="build/linux-release" ;;
    debug)   PRESET="linux-debug";   BUILD_PATH="build/linux-debug" ;;
    *)
        echo "Unsupported BuildConfig: $BUILD_CONFIG (use Release or Debug)" >&2
        exit 2
        ;;
esac

if [ ! -d "$BUILD_PATH" ]; then
    echo "Configuring $BUILD_CONFIG version..."
    if [ "$PRESET" = "linux-release" ]; then
        cmake --preset "$PRESET" -DRAYWAVES_DISTRIBUTION_BUILD=ON
    else
        cmake --preset "$PRESET"
    fi
fi

echo "Building targets (game runtime and editor)..."
cmake --build "$BUILD_PATH" --config "$BUILD_CONFIG" --target game main

# Verify expected outputs exist before packaging
GAME_EXE="$BUILD_PATH/game"
EDITOR_EXE="$BUILD_PATH/RayWaves"
RAYLIB_SO="$BUILD_PATH/libraylib.so"

if [ ! -f "$GAME_EXE" ]; then
    echo "Missing game at $GAME_EXE" >&2
    exit 1
fi
if [ ! -f "$RAYLIB_SO" ]; then
    echo "Missing libraylib.so at $RAYLIB_SO" >&2
    exit 1
fi

# Create distribution directory structure
DIST_PATH="$OUTPUT_DIR"
if [ -d "$DIST_PATH" ]; then
    echo "Cleaning old distribution..."
    rm -rf "$DIST_PATH"
fi

mkdir -p "$DIST_PATH/Core/Engine"
mkdir -p "$DIST_PATH/Documentation"
mkdir -p "$DIST_PATH/Core/raylib/include"
mkdir -p "$DIST_PATH/Core/raylib/lib"
mkdir -p "$DIST_PATH/Core/raylib/bin"

echo "Copying executable and dependencies..."

# Stop running instances so files are not busy. Patterns are path-qualified,
# so only processes launched from this dist/build tree are matched.
pkill -f "$DIST_PATH/game" 2>/dev/null || true
pkill -f "$DIST_PATH/RayWaves" 2>/dev/null || true
pkill -f "$BUILD_PATH/game" 2>/dev/null || true
pkill -f "$BUILD_PATH/RayWaves" 2>/dev/null || true

# Copy game runtime as hidden engine base for exports
# (Editor/ExportService.cpp probes Core/runtime)
cp -f "$GAME_EXE" "$DIST_PATH/Core/runtime"

# Optionally include the editor
if [ -f "$EDITOR_EXE" ]; then
    cp -f "$EDITOR_EXE" "$DIST_PATH/RayWaves"
fi

# Copy static Engine library to Core (needed when GameLogic links Engine)
if [ -f "$BUILD_PATH/libEngine.a" ]; then
    cp -f "$BUILD_PATH/libEngine.a" "$DIST_PATH/Core/"
fi

# Copy the whole libraylib chain (real file + soname + alias links) so both
# linking (-lraylib) and runtime NEEDED resolution work from the dist tree.
cp -a "$BUILD_PATH"/libraylib.so* "$DIST_PATH/Core/raylib/bin/"
if ls "$BUILD_PATH/_deps/raylib-build/raylib/include/"*.h >/dev/null 2>&1; then
    cp -f "$BUILD_PATH/_deps/raylib-build/raylib/include/"*.h \
        "$DIST_PATH/Core/raylib/include/"
fi

# Copy the chain to dist root so runtime and editor resolve it beside
# their own executable.
cp -a "$BUILD_PATH"/libraylib.so* "$DIST_PATH/"

# Core/runtime lives one level deeper; its $ORIGIN is dist/Core. Also the
# export flow looks for the chain beside Core/runtime first.
cp -a "$BUILD_PATH"/libraylib.so* "$DIST_PATH/Core/"

# Copy Engine UI Assets
mkdir -p "$DIST_PATH/Core/EngineContent"
cp -rf EngineContent/. "$DIST_PATH/Core/EngineContent/"

echo "Creating development environment..."

# Copy Engine headers and source files (needed for GameLogic development)
cp -f Engine/*.h "$DIST_PATH/Core/Engine/"
cp -f Engine/*.cpp "$DIST_PATH/Core/Engine/"

# Copy the distribution CMakeLists.txt
cp -f Distribution/dist_CMakeLists.txt "$DIST_PATH/Core/CMakeLists.txt"

# Copy distribution documentation
for doc in GAME_DEVELOPER_GUIDE.md GUIDE_FUNDAMENTALS.md GUIDE_REFERENCE.md; do
    if [ -f "Documentation/$doc" ]; then
        cp -f "Documentation/$doc" "$DIST_PATH/Documentation/"
    fi
done

# Copy Project Templates
if [ -d "Distribution/Templates" ]; then
    echo "Copying Project Templates..."
    cp -r Distribution/Templates "$DIST_PATH/"
    # Strip local build artifacts and .raywaves folders
    find "$DIST_PATH/Templates" \( -name '*.so' -o -name '*.a' \
        -o -name '*.o' -o -name '*.d' \) -type f -delete 2>/dev/null || true
    find "$DIST_PATH/Templates" -type d -name '.raywaves' \
        -prune -exec rm -rf {} + 2>/dev/null || true
fi

# Copy default game configuration
cp -f Distribution/config.ini "$DIST_PATH/"

echo "Distribution created successfully in '$DIST_PATH'"
echo ""
echo "Distribution contents:"
if [ -f "$DIST_PATH/RayWaves" ]; then
    echo "- RayWaves (Game Editor/IDE)"
fi
echo "- libraylib.so (required at runtime)"
echo "- config.ini (window and game settings)"
echo "- Templates/ (project templates)"
echo "- Documentation/ (user guides and documentation)"
echo "- Core/ (engine internals)"
echo "  - raylib/ (raylib development files)"
echo "  - CMakeLists.txt (for building GameLogic.so)"
echo "  - Engine/ (engine headers + sources)"
