#!/bin/bash
set -e

APP_NAME="DropAndDrag"
BUILD_DIR="build"
EXE="${BUILD_DIR}/${APP_NAME}"
DIST_DIR="dist"
DMG_DIR="${DIST_DIR}/dmgroot"
DMG_PATH="${DIST_DIR}/${APP_NAME}.dmg"
SKIA_DIR="${SKIA_DIR:-}"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}==> ${APP_NAME} macOS build${NC}"

# ---- dependencies ----
echo -e "${CYAN}==> Checking dependencies${NC}"
command -v cmake >/dev/null 2>&1 || { echo -e "${RED}cmake not found. Install: brew install cmake${NC}"; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo -e "${RED}ninja not found. Install: brew install ninja${NC}"; exit 1; }

# ---- skia check ----
if [ -z "$SKIA_DIR" ]; then
    if [ -d "/opt/homebrew/opt/skia" ]; then
        SKIA_DIR="/opt/homebrew/opt/skia"
    elif [ -d "/usr/local/opt/skia" ]; then
        SKIA_DIR="/usr/local/opt/skia"
    elif [ -d "$HOME/skia" ]; then
        SKIA_DIR="$HOME/skia"
    fi
fi

if [ -z "$SKIA_DIR" ]; then
    echo -e "${RED}SKIA_DIR not set and Skia not found.${NC}"
    echo "Build Skia first or set SKIA_DIR:"
    echo "  export SKIA_DIR=/path/to/skia"
    echo "See README.md for Skia build instructions."
    exit 1
fi

echo -e "${GREEN}  Skia: ${SKIA_DIR}${NC}"

# ---- configure ----
echo -e "${CYAN}==> Configuring${NC}"
cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSKIA_DIR="$SKIA_DIR" \
    -DDROPANDDRAG_BUILD_TESTS=OFF

# ---- build ----
echo -e "${CYAN}==> Building${NC}"
cmake --build "$BUILD_DIR"

# ---- tests ----
if [ -f "${BUILD_DIR}/tests/test_item" ]; then
    echo -e "${CYAN}==> Running tests${NC}"
    cd "$BUILD_DIR" && ctest --output-on-failure && cd ..
fi

echo -e "${GREEN}==> Build successful: ${BUILD_DIR}/${APP_NAME}${NC}"
echo ""
echo "To create .app bundle and DMG:"
echo "  ./packaging/macos/create_dmg.sh"
