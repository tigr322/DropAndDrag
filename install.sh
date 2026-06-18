#!/bin/bash
set -e
echo "DropAndDrag — one-command install"
echo "================================"

VERSION="${1:-1.0.2}"
DMG="DropAndDrag-${VERSION}-macOS.dmg"
URL="https://github.com/tigr322/DropAndDrag/releases/download/v${VERSION}/${DMG}"

echo "Downloading ${DMG}..."
curl -L -o /tmp/${DMG} "${URL}"

echo "Mounting DMG..."
hdiutil attach /tmp/${DMG} -nobrowse -quiet
sleep 1

echo "Installing to /Applications..."
cp -R /Volumes/DropAndDrag/DropAndDrag.app /Applications/

echo "Removing quarantine..."
xattr -dr com.apple.quarantine /Applications/DropAndDrag.app 2>/dev/null || true

echo "Registering with Launch Services..."
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f /Applications/DropAndDrag.app 2>/dev/null || true

hdiutil detach /Volumes/DropAndDrag -quiet
rm -f /tmp/${DMG}

echo ""
echo "✅ DropAndDrag installed to /Applications"
echo ""
echo "📌 Open: open /Applications/DropAndDrag.app"
echo "🔑 Grant Accessibility: System Settings → Privacy & Security → Accessibility → enable DropAndDrag"
echo "🖱️  Use: shake mouse while dragging a file"
