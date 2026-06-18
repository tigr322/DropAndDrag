#!/bin/bash
set -e
echo "========================================="
echo "  DropAndDrag — one-command install"
echo "========================================="
echo ""

APP="/Applications/DropAndDrag.app"
VERSION="${1:-1.0.2}"
DMG="DropAndDrag-${VERSION}-macOS.dmg"
URL="https://github.com/tigr322/DropAndDrag/releases/download/v${VERSION}/${DMG}"

if [ -d "$APP" ]; then
    echo "DropAndDrag already installed. Replacing..."
    rm -rf "$APP"
fi

echo "📦 Downloading..."
curl -fsSL -o "/tmp/${DMG}" "${URL}"

echo "📂 Mounting..."
hdiutil attach "/tmp/${DMG}" -nobrowse -quiet
sleep 1

echo "📁 Installing to /Applications..."
cp -R /Volumes/DropAndDrag/DropAndDrag.app /Applications/

hdiutil detach /Volumes/DropAndDrag -quiet 2>/dev/null || true
rm -f "/tmp/${DMG}"

echo "🔓 Removing quarantine..."
sudo xattr -dr com.apple.quarantine "$APP" 2>/dev/null || true

echo "📋 Registering..."
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$APP" 2>/dev/null || true

echo ""
echo "✅ Installed!"
echo ""
echo "🔑 Opening Accessibility settings..."
echo "   → Enable the checkbox next to DropAndDrag"
echo "   → If it's not in the list, click + and add /Applications/DropAndDrag.app"
echo ""
open "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"
sleep 2

echo "🚀 Launching DropAndDrag..."
open "$APP"

echo ""
echo "🖱️  How to use: hold a file + shake your mouse"
echo "📌 Tray icon: menu bar (top-right) — click for Show/Hide"
