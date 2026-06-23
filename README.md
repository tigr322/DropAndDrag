<picture>
  <source media="(prefers-color-scheme: dark)" srcset="resources/icons/icon.svg">
  <img alt="DropAndDrag" src="resources/icons/icon.svg" width="96">
</picture>

# DropAndDrag

> A lightweight drag-and-drop shelf. Drop files, folders, images, text, and URLs — pick them up anywhere. macOS + Linux.

- **Shake while dragging a file** to open the shelf and drop it inside
- Drag items **out** to any app or folder
- **Cmd+Click** to select multiple items and drag them all at once
- Shelf hides while you drag out, reappears when the drag ends
- Always on top, never steals focus

## Download

**Homebrew(Recommended):**
```bash
brew trust tigr322/tap
brew tap tigr322/tap
brew install --cask dropanddrag
```

**Direct DMG:** [DropAndDrag-1.0.11-macOS.dmg](https://github.com/tigr322/DropAndDrag/releases/download/v1.0.11/DropAndDrag-1.0.11-macOS.dmg) 
1. Open DMG, drag DropAndDrag.app to /Applications
2. sudo xattr -dr com.apple.quarantine /Applications/DropAndDrag.app
3. open /Applications/DropAndDrag.app
   (if blocked: System Settings → Privacy & Security → Open Anyway)
4. System Settings → Accessibility → enable DropAndDrag (for shake)

## Features

- **Drop anything** — files, folders, images, plain text, URLs
- **Real icons** — file icons via system API, image thumbnails via QuickLook, favicons for URLs
- **Multi-select** — Cmd+Click tiles to select multiple items; drag them all out at once
- **Auto-sizing** — shelf grows as you add items, resets when cleared
- **Shake-to-open** — shake the mouse while dragging a file to summon the shelf near the cursor; ignored when the shelf is already visible
- **System tray** — Show/Hide and Quit from the menu bar icon
- **Persistent settings** — window position and preferences saved to disk
- **Zero telemetry** — no network access, no cloud, everything stays local

## How it works

1. Drag a file (or text, URL, image) onto the shelf — it appears as a tile with its real icon
2. Keep working. The shelf floats above all windows and never steals focus
3. When you're ready, drag the tile to its destination
4. The shelf hides while you drag out so it doesn't obstruct the drop target, then reappears automatically
5. To move the shelf, drag its background. To bring it back when hidden, shake the mouse while dragging a file

## Technical details

| | Detail |
|---|---|---|
| **Language** | C++23 (ObjC++ on macOS, Xlib on Linux) |
| **Rendering** | macOS: AppKit/CoreGraphics · Linux: Xlib + freedesktop icon themes |
| **Thumbnails** | macOS: `QLThumbnailGenerator` · Linux: stb_image |
| **File icons** | macOS: `NSWorkspace iconForFile:` · Linux: freedesktop icon theme lookup |
| **Drag-out** | macOS: `NSDraggingSource` · Linux: XDnD source protocol |
| **Drag-in** | macOS: `NSDraggingDestination` · Linux: XDnD target protocol |
| **Settings** | JSON file in `~/Library/Application Support/DropAndDrag` (macOS) or `~/.config/dropanddrag` (Linux) |
| **Binary size** | ~1 MB (macOS) |
| **Platforms** | macOS 13+ · Linux (X11 + XWayland) |

## Building from source

### macOS

#### Prerequisites

- macOS 13+, Xcode Command Line Tools
- CMake 3.25+, Ninja
- SQLite3 (system)
- Skia (linked, required for build — see [Building Skia](#building-skia))

#### Quick build

```bash
git clone https://github.com/tigr322/DropAndDrag.git
cd DropAndDrag
./build.sh
```

Or step by step:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKIA_DIR=/path/to/skia
cmake --build build
open build/DropAndDrag.app
```

### Linux

#### Prerequisites

```bash
# Debian/Ubuntu
sudo apt install cmake ninja-build pkg-config \
  libx11-dev libxi-dev libxrender-dev libxext-dev libxtst-dev \
  wayland-protocols libwayland-dev libgl1-mesa-dev zlib1g-dev

# Fedora
sudo dnf install cmake ninja-build pkg-config \
  libX11-devel libXi-devel libXrender-devel libXext-devel libXtst-devel \
  wayland-protocols wayland-devel mesa-libGL-devel zlib-devel

# Arch
sudo pacman -S cmake ninja pkgconf libx11 libxi libxrender libxext libxtst \
  wayland wayland-protocols mesa zlib
```

For real file icons (not colored fallback tiles), install an icon theme:
```bash
sudo apt install adwaita-icon-theme    # Debian/Ubuntu
sudo dnf install adwaita-icon-theme    # Fedora
sudo pacman -S adwaita-icon-theme      # Arch
```

#### Build

```bash
git clone https://github.com/tigr322/DropAndDrag.git
cd DropAndDrag
./build_linux.sh
./build/DropAndDrag
```

The build script auto-detects Skia at `~/skia`, `~/Projects/skia`, `/opt/skia`, or `/usr/local/skia`. Override with `SKIA_DIR`:
```bash
SKIA_DIR=/custom/path ./build_linux.sh
```

### Build options

| Option | Default | Description |
|---|---|---|
| `DROPANDDRAG_BUILD_TESTS` | `ON` | Build unit tests |
| `DROPANDDRAG_ENABLE_SANITIZERS` | `ON` | ASan/UBSan in debug builds |
| `DROPANDDRAG_ENABLE_LTO` | `ON` | Link-time optimization in release |
| `SKIA_DIR` | — | Path to Skia checkout root |

### Building Skia

```bash
git clone https://skia.googlesource.com/skia.git
cd skia
python3 tools/git-sync-deps

bin/gn gen out/Release --args='
  is_official_build=true
  skia_use_metal=true
  skia_use_system_libjpeg_turbo=false
  skia_use_system_libwebp=false
  skia_use_system_libpng=false
  skia_use_system_icu=false
  skia_use_system_harfbuzz=false
  skia_use_system_expat=false
  skia_enable_skottie=false
  skia_enable_svg=false
  skia_enable_pdf=false
  target_cpu="arm64"
'
ninja -C out/Release
```

Set `SKIA_DIR` to the skia checkout root:
```bash
cmake -B build -DSKIA_DIR=/path/to/skia
```

### Running tests

```bash
cmake --build build && cd build && ctest --output-on-failure
```

## Architecture

```
src/
├── core/              # Platform-independent: Item model, Database, EventBus, Settings
├── platform/          # Abstract interfaces: NativeWindow, DragDrop, Hotkeys, Tray
├── ui/                # Shelf rendering, item icons, layout
│   ├── renderer.mm    #   macOS: AppKit/CoreGraphics rendering
│   └── ui_linux.cpp   #   Linux: Xlib rendering + freedesktop icon theme
├── platform_impl/
│   ├── macos/         # Cocoa implementation (.mm)
│   ├── windows/       # Win32 implementation
│   └── linux/         # X11/Xdnd/XEMBED/inotify implementation
└── app/               # Application lifecycle, event loop, signal handling
```

- **Main thread** — Event loop + rendering
- **Linux rendering** — Xlib via dedicated `Display` connection (`ui_linux.cpp`)

## Linux architecture

### Where everything lives

| Component | File | What it does |
|---|---|---|
| Window + events | `src/platform_impl/linux/window_linux.cpp` | X11 native window, XDnD target/source, keyboard, mouse |
| Drag-out | `src/platform_impl/linux/window_linux.cpp` | XDnD source protocol: `findXdndWindow`, `sendXdndDropToTarget`, `handleSelectionRequestOut` |
| Drag-in | `src/platform_impl/linux/window_linux.cpp` | `XDndHandler` class — XdndEnter/Position/Drop/Leave |
| Mouse monitor | `src/platform_impl/linux/mouse_monitor_linux.cpp` | XRecord + Wayland relative pointer + `/dev/input/mice` fallback |
| Shake detector | `src/core/mouse_shake/mouse_shake_detector.cpp` | Platform-agnostic algorithm |
| Shelf rendering | `src/ui/ui_linux.cpp` | Xlib drawing, freedesktop icon lookup, stb_image thumbnails |
| Icon loading | `src/ui/ui_linux.cpp` | `find_icon_named` — theme search, `load_icon_pixmap` — Pixmap cache |
| stb_image | `src/vendor/stb_image.h` | Single-header PNG/JPEG/BMP/GIF loader |
| Clipboard | `src/platform_impl/linux/clipboard_linux.cpp` | X11 selections (CLIPBOARD, PRIMARY) |
| File monitor | `src/platform_impl/linux/file_monitor_linux.cpp` | inotify watcher with debounce |
| Hotkeys | `src/platform_impl/linux/hotkeys_linux.cpp` | XGrabKey with NumLock/CapsLock variants |
| System tray | `src/platform_impl/linux/tray_linux.cpp` | XEMBED protocol, notify-send fallback |
| Notifications | `src/platform_impl/linux/notifications_linux.cpp` | DBus notifications |
| Event loop | `src/app/application.cpp:561` | `run_linux_loop()` — 4ms tick |
| Build script | `build_linux.sh` | Dependency check + configure + build |

### Drag-out pipeline

```
ButtonPress on tile → hitTestItem (window coords → item index)
MotionNotify > 5px → beginItemDrag
  ├── XSetSelectionOwner (XdndSelection)
  ├── XChangeProperty (text/uri-list with file://path)
  ├── XGrabPointer (track all motion)
  ├── set_drag_out_active(true) — blocks shake detector
  └── hide() — shelf disappears
MotionNotify (during drag) → tracks root_x,root_y
ButtonRelease → completeItemDrag
  ├── XUngrabPointer
  ├── findXdndWindow (XTranslateCoordinates → top-level windows → XdndAware check)
  ├── sendXdndDropToTarget (XdndEnter → XdndPosition → XdndDrop)
  ├── Wait for SelectionRequest → serve file:// URI
  ├── Wait for XdndFinished
  ├── set_drag_out_active(false)
  └── show() — shelf reappears
```

### Freedesktop icon loading

```
Item.path → stat() (folder?) → is_image_ext? (thumbnail) → ext_to_mime → mime_to_icon_names
  → find_icon_named (search /usr/share/icons/{Adwaita,hicolor,gnome,…}/{48x48,scalable,…}/{mimetypes,places,…})
  → stbi_load → downscale_rgba → composite_rgba → XCreateImage → XPutImage → Pixmap cache
  → XCopyArea (each render frame)
Fallback: colored tile with letter (F=file, D=folder, I=image, U=URL, T=text)
```

### Mouse monitoring on Linux

The shake detector needs global mouse tracking across all applications. On Linux, three backends are tried in order:

1. **Wayland `zwp_relative_pointer_manager_v1`** — broadcasts relative motion to all clients regardless of focus. Works over Wayland-native windows (Nautilus GTK4, etc.).
2. **X11 `XRecord` extension** — captures `MotionNotify` events from the X server wire protocol. Needs `libxtst-dev`. Works on X11 + XWayland.
3. **`/dev/input/mice` PS/2 protocol** — reads raw 3-byte packets. Legacy fallback, often empty on evdev systems.
4. **`XQueryPointer`** — absolute position polling every 4ms. Last resort.

### Known Linux limitations

- **Wayland drag-out**: XDnD only reaches X11/XWayland windows. Dragging to Wayland-native apps (e.g., Nautilus on pure Wayland) requires compositor-level protocol implementation.
- **System tray**: XEMBED is deprecated on modern desktops. Falls back to `notify-send` when no tray is running. AppIndicator/KStatusNotifierItem not yet implemented.
- **File icons**: Requires an icon theme package (`adwaita-icon-theme` or similar). Without it, falls back to colored tiles.
- **Image thumbnails**: QuickLook-equivalent (thumbnailing server) not implemented. Image files show as thumbnails using stb_image; PDF/video show MIME-type icon.
- **Multi-select**: Cmd+Click not yet implemented on Linux (only single-item drag-out).

## License

MIT © [DropAndDrag contributors](https://github.com/tigr322/DropAndDrag/graphs/contributors)

---

<p align="center">
  <sub>Built with C++23 · AppKit · SQLite3 · CMake</sub>
</p>
