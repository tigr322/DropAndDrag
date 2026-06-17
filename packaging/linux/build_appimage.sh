#!/bin/bash
set -euo pipefail

APP_NAME="DropAndDrag"
VERSION="${VERSION:-1.0.0}"
BUILD_DIR="${BUILD_DIR:-build}"
SRC_DIR="$(dirname "$0")/../.."
APPDIR="$(mktemp -d)/${APP_NAME}.AppDir"
RESOURCES_DIR="${SRC_DIR}/resources"

echo "=== Building ${APP_NAME} v${VERSION} for Linux ==="

cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -G Ninja

cmake --build "${BUILD_DIR}" --config Release --target DropAndDrag

echo "=== Creating AppDir structure ==="

mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${APPDIR}/usr/share/metainfo"

cp "${BUILD_DIR}/DropAndDrag" "${APPDIR}/usr/bin/${APP_NAME}"
chmod +x "${APPDIR}/usr/bin/${APP_NAME}"

deps=$(ldd "${APPDIR}/usr/bin/${APP_NAME}" | awk '/=> \// {print $3}' | grep -v 'libc\.\|libpthread\|libdl\|ld-linux' || true)
for dep in ${deps}; do
    cp "${dep}" "${APPDIR}/usr/lib/" 2>/dev/null || true
done

if [ -f "${RESOURCES_DIR}/icons/icon.svg" ]; then
    cp "${RESOURCES_DIR}/icons/icon.svg" "${APPDIR}/${APP_NAME}.svg"
    cp "${RESOURCES_DIR}/icons/icon.svg" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.svg"
    if command -v rsvg-convert &> /dev/null; then
        rsvg-convert -w 256 -h 256 "${RESOURCES_DIR}/icons/icon.svg" \
            -o "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
    fi
fi

cat > "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" << EOF
[Desktop Entry]
Type=Application
Name=DropAndDrag
Comment=Fast cross-platform drag-and-drop shelf utility
Exec=${APP_NAME}
Icon=${APP_NAME}
Categories=Utility;Office;
Terminal=false
StartupNotify=true
X-AppImage-Version=${VERSION}
EOF

cat > "${APPDIR}/usr/share/metainfo/${APP_NAME}.appdata.xml" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>com.dropanddrag.app</id>
  <name>DropAndDrag</name>
  <summary>Drag-and-drop shelf utility</summary>
  <description>
    <p>Fast cross-platform drag-and-drop shelf utility for quick access to files,
    folders, images, text snippets, and URLs.</p>
  </description>
  <url type="homepage">https://github.com/dropanddrag/dropanddrag</url>
  <releases>
    <release version="${VERSION}" date="$(date +%Y-%m-%d)"/>
  </releases>
</component>
EOF

cat > "${APPDIR}/AppRun" << 'APPRUNEOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
exec "${HERE}/usr/bin/DropAndDrag" "$@"
APPRUNEOF
chmod +x "${APPDIR}/AppRun"

echo "=== Downloading appimagetool ==="

APPIMAGETOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
APPIMAGETOOL="$(mktemp -d)/appimagetool"

if command -v appimagetool &> /dev/null; then
    APPIMAGETOOL="appimagetool"
elif ! command -v wget &> /dev/null && ! command -v curl &> /dev/null; then
    echo "Neither wget nor curl found. Cannot download appimagetool."
    echo "AppDir created at: ${APPDIR}"
    echo "Install appimagetool manually and run: appimagetool ${APPDIR}"
    exit 1
else
    if command -v wget &> /dev/null; then
        wget -q "${APPIMAGETOOL_URL}" -O "${APPIMAGETOOL}"
    else
        curl -sSL "${APPIMAGETOOL_URL}" -o "${APPIMAGETOOL}"
    fi
    chmod +x "${APPIMAGETOOL}"
fi

echo "=== Creating AppImage ==="

ARCH=x86_64
OUTPUT="${SRC_DIR}/${APP_NAME}-${VERSION}-${ARCH}.AppImage"

"${APPIMAGETOOL}" "${APPDIR}" "${OUTPUT}"

echo "=== Done: ${OUTPUT} ==="

rm -rf "$(dirname "${APPDIR}")"
