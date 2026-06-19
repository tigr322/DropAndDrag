<picture>
  <source media="(prefers-color-scheme: dark)" srcset="resources/icons/icon.svg">
  <img alt="DropAndDrag" src="resources/icons/icon.svg" width="96">
</picture>

# DropAndDrag

> A lightweight drag-and-drop shelf for macOS. Drop files, folders, images, text, and URLs — pick them up anywhere.

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
|---|---|
| **Language** | C++23 (Objective-C++ for macOS integration) |
| **Rendering** | AppKit / CoreGraphics (`CGContextRef`, `NSBezierPath`) |
| **Thumbnails** | `QLThumbnailGenerator` (async, macOS 10.15+) |
| **File icons** | `NSWorkspace iconForFile:` |
| **Favicons** | Google S2 favicon service (async via `NSURLSession`) |
| **Database** | SQLite3 with WAL mode |
| **Window** | `NSPanel` — borderless, non-activating, always on top |
| **Drag source** | `NSDraggingSource` with one `NSDraggingItem` per file |
| **Drag destination** | `NSDraggingDestination` (files, URLs, text, images) |
| **Settings** | JSON file in `~/Library/Application Support/DropAndDrag` |
| **Binary size** | ~1 MB |
| **Platforms** | macOS 13+ (Ventura). Windows and Linux planned. |

## Building from source

### Prerequisites

- macOS 13+, Xcode Command Line Tools
- CMake 3.25+, Ninja
- SQLite3 (system)
- Skia (linked, required for build — see [Building Skia](#building-skia))

### Quick build

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
├── ui/                # Shelf rendering (AppKit/CoreGraphics), item icons, layout
├── platform_impl/     # Cocoa implementation (.mm): window, tray, clipboard, hotkeys
└── app/               # Application lifecycle, event loop, signal handling
```

- **Main thread** — AppKit event loop + shelf rendering via `drawRect:`
- **DB thread** — SQLite3 queries, async via `std::future`
- **Worker pool** — thumbnail generation (`QLThumbnailGenerator`), favicon fetches
- **Mouse monitor thread** — shake detection via global event tap

## License

MIT © [DropAndDrag contributors](https://github.com/tigr322/DropAndDrag/graphs/contributors)

---

<p align="center">
  <sub>Built with C++23 · AppKit · SQLite3 · CMake</sub>
</p>
