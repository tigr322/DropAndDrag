#!/bin/bash
set -euo pipefail

APP_NAME="DropAndDrag"
VERSION="${VERSION:-1.0.0}"
BUILD_DIR="${BUILD_DIR:-build}"
STAGING_DIR="$(mktemp -d)/${APP_NAME}.app"
RESOURCES_DIR="$(dirname "$0")/../../resources"
PACKAGING_DIR="$(dirname "$0")"
SRC_DIR="$(dirname "$0")/../.."

echo "=== Building ${APP_NAME} v${VERSION} ==="

SKIA_DIR="${SKIA_DIR:-$HOME/skia}"
cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -G Ninja -DSKIA_DIR="${SKIA_DIR}"
cmake --build "${BUILD_DIR}" --config Release --target DropAndDrag

echo "=== Creating .app bundle ==="

APP_BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
if [ ! -d "${APP_BUNDLE}" ]; then
    echo "ERROR: ${APP_BUNDLE} not found. Run build.sh first."
    exit 1
fi

STAGING_DIR="$(mktemp -d)/${APP_NAME}.app"
cp -R "${APP_BUNDLE}" "${STAGING_DIR}"

BUNDLE_ID="com.dropanddrag.app"
if [ -n "${DEVELOPER_ID_APPLICATION:-}" ]; then
    echo "=== Signing .app bundle ==="
    codesign --force --deep --sign "${DEVELOPER_ID_APPLICATION}" \
        --options runtime \
        --entitlements "${PACKAGING_DIR}/entitlements.plist" \
        "${STAGING_DIR}"
fi

echo "=== Creating DMG ==="

mkdir -p dist
DMG_NAME="${APP_NAME}-${VERSION}-macOS.dmg"
DMG_PATH="dist/${DMG_NAME}"

rm -f "${DMG_PATH}"

hdiutil create \
    -volname "${APP_NAME}" \
    -srcfolder "$(dirname "${STAGING_DIR}")" \
    -ov \
    -format UDZO \
    -imagekey zlib-level=9 \
    "${DMG_PATH}"

if [ -n "${DEVELOPER_ID_APPLICATION:-}" ]; then
    echo "=== Signing DMG ==="
    codesign --force --sign "${DEVELOPER_ID_APPLICATION}" "${DMG_PATH}"
fi

if [ -n "${NOTARIZATION_USERNAME:-}" ] && [ -n "${NOTARIZATION_PASSWORD:-}" ]; then
    echo "=== Notarizing DMG ==="
    xcrun notarytool submit "${DMG_PATH}" \
        --apple-id "${NOTARIZATION_USERNAME}" \
        --password "${NOTARIZATION_PASSWORD}" \
        --team-id "${NOTARIZATION_TEAM_ID:-}" \
        --wait

    xcrun stapler staple "${DMG_PATH}"
fi

echo "=== Done: ${DMG_PATH} ==="

rm -rf "$(dirname "${STAGING_DIR}")"
