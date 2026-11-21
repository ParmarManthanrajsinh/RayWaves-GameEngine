#!/usr/bin/env bash
# setup_tools.sh - fetch the pinned CMake/Ninja toolchain on Linux.
#
# Linux uses the system GCC/G++ so host and GameLogic share libstdc++. Tool
# versions live in Tools/toolchain.ini. Windows still fetches Zig through
# Tools/setup_tools.ps1.
#
# Usage:
#   Tools/setup_tools.sh [--skip-zig] [--skip-ninja] [--skip-cmake] [--skip-rcedit]
#
# Idempotent: an already-downloaded tool is left untouched.

set -euo pipefail

TOOLS_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
VERSIONS_FILE="$TOOLS_DIR/toolchain.ini"

if [ ! -f "$VERSIONS_FILE" ]; then
    echo "error: missing $VERSIONS_FILE" >&2
    exit 1
fi

read_version() {
    local key=$1
    local value
    value=$(sed -n "s/^[[:space:]]*${key}[[:space:]]*=[[:space:]]*//p" "$VERSIONS_FILE" | tail -n 1 | tr -d '\r')
    if [ -z "$value" ]; then
        echo "error: '$key' missing from $VERSIONS_FILE" >&2
        exit 1
    fi
    printf '%s' "$value"
}

read_optional() {
    sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "$VERSIONS_FILE" | tail -n 1 | tr -d '\r'
}

CMAKE_VERSION=$(read_version cmake_version)
NINJA_VERSION=$(read_version ninja_version)

SKIP_ZIG=0
SKIP_NINJA=0
SKIP_CMAKE=0
SKIP_RCEDIT=0

while [ $# -gt 0 ]; do
    case "$1" in
        --skip-zig) SKIP_ZIG=1 ;;
        --skip-ninja) SKIP_NINJA=1 ;;
        --skip-cmake) SKIP_CMAKE=1 ;;
        --skip-rcedit) SKIP_RCEDIT=1 ;; # Windows-only; accepted for parity
        -h|--help)
            sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            exit 2
            ;;
    esac
    shift
done

host_arch() {
    case "$(uname -m)" in
        x86_64|amd64) printf 'x86_64' ;;
        aarch64|arm64) printf 'aarch64' ;;
        *) uname -m ;;
    esac
}

ARCH=$(host_arch)
if [ "$ARCH" != "x86_64" ]; then
    echo "error: RayWaves Linux packages currently support x86_64 only (found $ARCH)." >&2
    exit 1
fi

CMAKE_SHA256=$(read_optional "cmake_sha256_linux_${ARCH}")
NINJA_SHA256=$(read_optional "ninja_sha256_linux_${ARCH}")

DOWNLOAD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/raywaves-tools.XXXXXX")
trap 'rm -rf "$DOWNLOAD_DIR"' EXIT

download() {
    local url=$1
    local dest=$2
    echo "Downloading $(basename "$dest")..."
    if ! curl -fL --retry 3 --retry-delay 1 -o "$dest" "$url"; then
        echo "error: failed to download $url" >&2
        exit 1
    fi
}

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        return 1
    fi
}

verify_sha256() {
    local archive=$1
    local expected=$2
    [ -n "$expected" ] || return 0
    local actual
    if ! actual=$(sha256_of "$archive"); then
        echo "warning: no SHA-256 tool; skipping checksum for $(basename "$archive")" >&2
        return 0
    fi
    if [ "$actual" != "$expected" ]; then
        echo "error: checksum mismatch for $(basename "$archive")" >&2
        echo "  expected: $expected" >&2
        echo "  actual:   $actual" >&2
        exit 1
    fi
}

extract_tar() {
    local archive=$1
    local dest=$2
    mkdir -p "$dest"
    tar -xf "$archive" -C "$dest"
}

extract_zip() {
    local archive=$1
    local dest=$2
    mkdir -p "$dest"
    if command -v unzip >/dev/null 2>&1; then
        unzip -q "$archive" -d "$dest"
    elif command -v bsdtar >/dev/null 2>&1; then
        bsdtar -xf "$archive" -C "$dest"
    elif [ -x "$TOOLS_DIR/cmake/bin/cmake" ]; then
        (cd "$dest" && "$TOOLS_DIR/cmake/bin/cmake" -E tar xf "$archive")
    elif command -v cmake >/dev/null 2>&1; then
        (cd "$dest" && cmake -E tar xf "$archive")
    else
        echo "error: need unzip, bsdtar, or cmake to extract $(basename "$archive")" >&2
        exit 1
    fi
}

rename_dir() {
    local from=$1
    local to=$2
    if [ -d "$from" ] && [ "$from" != "$to" ]; then
        rm -rf "$to"
        mv "$from" "$to"
    fi
}

# --- CMake -----------------------------------------------------------------
if [ "$SKIP_CMAKE" -eq 0 ]; then
    if [ -x "$TOOLS_DIR/cmake/bin/cmake" ]; then
        echo "CMake is already downloaded."
    else
        echo "Downloading CMake $CMAKE_VERSION..."
        CMAKE_ARCHIVE="$DOWNLOAD_DIR/cmake.tar.gz"
        download "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${ARCH}.tar.gz" "$CMAKE_ARCHIVE"
        verify_sha256 "$CMAKE_ARCHIVE" "$CMAKE_SHA256"
        extract_tar "$CMAKE_ARCHIVE" "$DOWNLOAD_DIR"
        rename_dir "$DOWNLOAD_DIR/cmake-${CMAKE_VERSION}-linux-${ARCH}" "$TOOLS_DIR/cmake"
        chmod +x "$TOOLS_DIR/cmake/bin/cmake" "$TOOLS_DIR/cmake/bin/ctest" "$TOOLS_DIR/cmake/bin/cpack"
    fi
fi

# --- Ninja -----------------------------------------------------------------
if [ "$SKIP_NINJA" -eq 0 ]; then
    if [ -x "$TOOLS_DIR/ninja/ninja" ]; then
        echo "Ninja is already downloaded."
    else
        echo "Downloading Ninja $NINJA_VERSION..."
        NINJA_ARCHIVE="$DOWNLOAD_DIR/ninja.zip"
        download "https://github.com/ninja-build/ninja/releases/download/v${NINJA_VERSION}/ninja-linux.zip" "$NINJA_ARCHIVE"
        verify_sha256 "$NINJA_ARCHIVE" "$NINJA_SHA256"
        mkdir -p "$TOOLS_DIR/ninja"
        extract_zip "$NINJA_ARCHIVE" "$TOOLS_DIR/ninja"
        chmod +x "$TOOLS_DIR/ninja/ninja"
    fi
fi

# rcedit is a Windows PE resource editor; nothing to fetch on Linux.
if [ "$SKIP_RCEDIT" -eq 0 ]; then
    :
fi

echo "Setup complete. Tools are ready."
