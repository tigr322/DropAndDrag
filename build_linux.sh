#!/bin/bash
set -e

APP_NAME="DropAndDrag"
BUILD_DIR="build"
SKIA_DIR="${SKIA_DIR:-}"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}==> ${APP_NAME} Linux build${NC}"

# ---- system dependencies ----
echo -e "${CYAN}==> Checking system dependencies${NC}"
MISSING=()
command -v cmake  >/dev/null 2>&1 || MISSING+=("cmake")
command -v ninja  >/dev/null 2>&1 || MISSING+=("ninja-build")
pkg-config --exists x11    2>/dev/null || MISSING+=("libx11-dev")
pkg-config --exists xi     2>/dev/null || MISSING+=("libxi-dev")
pkg-config --exists gl     2>/dev/null || MISSING+=("libgl1-mesa-dev")
pkg-config --exists zlib   2>/dev/null || MISSING+=("zlib1g-dev")
if [ ${#MISSING[@]} -gt 0 ]; then
    echo -e "${RED}Missing packages: ${MISSING[*]}${NC}"
    echo "Install with:"
    echo "  sudo apt install ${MISSING[*]}"
    exit 1
fi
echo -e "${GREEN}  All dependencies found${NC}"

# ---- skia check ----
if [ -z "$SKIA_DIR" ]; then
    for d in "$HOME/skia" "$HOME/Projects/skia" "/opt/skia" "/usr/local/skia"; do
        [ -d "$d" ] && SKIA_DIR="$d" && break
    done
fi

if [ -z "$SKIA_DIR" ]; then
    echo -e "${RED}SKIA_DIR not set and Skia not found.${NC}"
    echo "Build Skia for Linux first:"
    echo "  git clone https://skia.googlesource.com/skia.git"
    echo "  cd skia && python3 tools/git-sync-deps"
    echo "  bin/gn gen out/Release --args='is_official_build=true'"
    echo "  ninja -C out/Release"
    echo "  export SKIA_DIR=\$PWD"
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

echo -e "${GREEN}==> Build successful: ${BUILD_DIR}/${APP_NAME}${NC}"
echo ""
echo "Run with:"
echo "  ./${BUILD_DIR}/${APP_NAME}"
echo ""
echo "Install system-wide:"
echo "  sudo cp ${BUILD_DIR}/${APP_NAME} /usr/local/bin/"
echo "  sudo chmod +x /usr/local/bin/${APP_NAME}"
