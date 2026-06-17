#!/bin/bash
set -e

VERSION="${1:-1.0.0}"
GITHUB_REPO="anomalyco/DropAndDrag"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

if [ -z "$GITHUB_TOKEN" ]; then
    echo -e "${RED}GITHUB_TOKEN not set. Export it:${NC}"
    echo "  export GITHUB_TOKEN=ghp_..."
    exit 1
fi

echo -e "${CYAN}==> DropAndDrag Release v${VERSION}${NC}"
echo ""

# ---- step 1: build ----
echo -e "${CYAN}==> Step 1: Build${NC}"
./build.sh
echo ""

# ---- step 2: create DMG ----
echo -e "${CYAN}==> Step 2: Package DMG${NC}"
./packaging/macos/create_dmg.sh
DMG_FILE="dist/DropAndDrag.dmg"
if [ ! -f "$DMG_FILE" ]; then
    echo -e "${RED}DMG not found at ${DMG_FILE}${NC}"
    exit 1
fi
echo -e "${GREEN}  DMG: ${DMG_FILE}${NC}"
echo ""

# ---- step 3: sign (optional) ----
if [ -n "$APPLE_DEVELOPER_ID" ]; then
    echo -e "${CYAN}==> Step 3: Sign${NC}"
    codesign --deep --force --verify --verbose \
        --sign "$APPLE_DEVELOPER_ID" \
        "dist/dmgroot/DropAndDrag.app"
    echo -e "${GREEN}  Signed with: ${APPLE_DEVELOPER_ID}${NC}"
else
    echo -e "${CYAN}==> Step 3: Sign (skipped — no APPLE_DEVELOPER_ID)${NC}"
fi
echo ""

# ---- step 4: notarize (optional) ----
if [ -n "$APPLE_NOTARY_PROFILE" ]; then
    echo -e "${CYAN}==> Step 4: Notarize${NC}"
    ditto -c -k --keepParent "dist/dmgroot/DropAndDrag.app" dist/notarize.zip
    xcrun notarytool submit dist/notarize.zip \
        --keychain-profile "$APPLE_NOTARY_PROFILE" \
        --wait
    rm -f dist/notarize.zip
    echo -e "${GREEN}  Notarized${NC}"
else
    echo -e "${CYAN}==> Step 4: Notarize (skipped — no APPLE_NOTARY_PROFILE)${NC}"
fi
echo ""

# ---- step 5: compute checksums ----
echo -e "${CYAN}==> Step 5: Checksums${NC}"
DMG_SHA=$(shasum -a 256 "$DMG_FILE" | cut -d' ' -f1)
echo -e "${GREEN}  SHA256: ${DMG_SHA}${NC}"
echo ""

# ---- step 6: update cask formula ----
echo -e "${CYAN}==> Step 6: Update Homebrew cask${NC}"
CASK_FILE="homebrew/Casks/dropanddrag.rb"
if [[ "$(uname -m)" == "arm64" ]]; then
    sed -i '' "s/PLACEHOLDER_ARM64_SHA256/${DMG_SHA}/" "$CASK_FILE"
else
    sed -i '' "s/PLACEHOLDER_X64_SHA256/${DMG_SHA}/" "$CASK_FILE"
fi
echo -e "${GREEN}  Updated ${CASK_FILE}${NC}"
echo ""

# ---- step 7: create GitHub release ----
echo -e "${CYAN}==> Step 7: Create GitHub release${NC}"
gh release create "v${VERSION}" \
    --repo "$GITHUB_REPO" \
    --title "v${VERSION}" \
    --notes "See [CHANGELOG.md](https://github.com/${GITHUB_REPO}/blob/main/CHANGELOG.md)" \
    "$DMG_FILE" \
    "$DMG_FILE#DropAndDrag-${VERSION}.dmg"

echo ""
echo -e "${GREEN}==> Release v${VERSION} created!${NC}"
echo ""
echo "Next steps:"
echo ""
echo "  1. Push the updated cask to your tap repo:"
echo "     git -C homebrew add Casks/dropanddrag.rb"
echo "     git -C homebrew commit -m 'Update dropanddrag cask to v${VERSION}'"
echo "     git -C homebrew push"
echo ""
echo "  2. Users install with:"
echo "     brew tap anomalyco/tap"
echo "     brew install --cask dropanddrag"
echo ""
