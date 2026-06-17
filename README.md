<picture>
  <source media="(prefers-color-scheme: dark)" srcset="resources/icons/icon.svg">
  <img alt="DropAndDrag" src="resources/icons/icon.svg" width="96">
</picture>

# DropAndDrag

> The fastest cross-platform drag-and-drop shelf. Drop files, text, URLs, and images — pick them up anywhere.

- **Shake while dragging** to open the shelf and drop items inside *(optional)*
- Drag items **out** to move them
- Hold <kbd>Ctrl</kbd> (Windows/Linux) or <kbd>⌘ Command</kbd> (macOS) while dragging to copy
- Global hotkey to summon the shelf anytime
- Always on top, auto-hides when not in use

<p align="center">
  <img src="assets/shelf.png" alt="DropAndDrag shelf" width="640"><br>
  <sup>Floating shelf with files, text, images, and URLs</sup>
</p>

## Why DropAndDrag?

| | DropAndDrag | Tokri | Dropover |
|---|---|---|---|
| **Language** | C++23 | C++/Qt | Swift |
| **Startup** | &lt;50 ms | ~200 ms | ~300 ms |
| **Idle Memory** | &lt;15 MB | ~80 MB | ~120 MB |
| **Rendering** | Skia GPU | Qt Widgets | AppKit |
| **Native API** | Win32 / Cocoa / X11 | Qt abstractions | Cocoa |
| **Database** | SQLite3 (WAL) | SQLite3 | SQLite |
| **License** | MIT | MIT | Proprietary |

## Features

- **Universal basket** — files, folders, images, text, URLs, multiple items at once
- **Multiple collections** — Work, Personal, Temporary, custom shelves
- **Color tags** — tag items for quick filtering and organization
- **Favorites** — pin frequently used items
- **Quick Paste** — paste the last item via global hotkey
- **Instant search** — search by name, content, or type (&lt;10 ms)
- **History** — items persist between restarts, configurable retention
- **Glassmorphism UI** — modern translucent design, adapts per platform
- **Dark & Light themes** — follows system preference automatically
- **100% local** — no telemetry, no cloud, no network. Your data stays on your disk.

## Download

### Windows

- Installer: [DropAndDragSetup.exe](https://github.com/anomalyco/DropAndDrag/releases/latest/download/DropAndDragSetup.exe)
- Portable (.zip): [DropAndDrag.zip](https://github.com/anomalyco/DropAndDrag/releases/latest/download/DropAndDrag.zip)
- Install via Winget *(coming soon)*:
  ```powershell
  winget install DropAndDrag
  ```

### macOS

- DMG installer: [DropAndDrag.dmg](https://github.com/anomalyco/DropAndDrag/releases/latest/download/DropAndDrag.dmg)
- Install via Homebrew *(coming soon)*:
  ```bash
  brew install --cask dropanddrag
  ```

> **Note for macOS users**
>
> This app is **unsigned**. macOS will block it.
>
> To run it:
> ```bash
> sudo xattr -dr com.apple.quarantine /Applications/DropAndDrag.app
> ```
> Or allow it via: **System Settings → Privacy & Security → Open Anyway**

### Linux

- AppImage: [DropAndDrag.AppImage](https://github.com/anomalyco/DropAndDrag/releases/latest/download/DropAndDrag.AppImage)
- DEB package: [dropanddrag.deb](https://github.com/anomalyco/DropAndDrag/releases/latest/download/dropanddrag.deb)
- RPM package: [dropanddrag.rpm](https://github.com/anomalyco/DropAndDrag/releases/latest/download/dropanddrag.rpm)
- Install via Flatpak *(coming soon)*:
  ```bash
  flatpak install dropanddrag
  ```

> **Note for Linux users**
>
> For global hotkey support on X11, the app grabs key events from the X server. On Wayland, global hotkeys require compositor protocol support.
>
> If mouse shake activation is enabled, add your user to the `input` group:
> ```bash
> sudo usermod -aG input $USER
> ```
> Log out and log back in for the change to take effect.

## Building from source

### Prerequisites

| Dependency | macOS | Windows | Linux |
|---|---|---|---|
| **Compiler** | Clang 16+ (Xcode) | MSVC 2022+ / Clang 16+ | GCC 13+ / Clang 16+ |
| **CMake** | 3.25+ | 3.25+ | 3.25+ |
| **Ninja** | ✓ | ✓ | ✓ |
| **SQLite3** | system | system | libsqlite3-dev |
| **Skia** | [Build from source](#building-skia) | [Build from source](#building-skia) | [Build from source](#building-skia) |

### Quick build

```bash
# Clone
git clone https://github.com/anomalyco/DropAndDrag.git
cd DropAndDrag

# macOS / Linux
./build.sh

# Windows
build.bat
```

#### Or step by step

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKIA_DIR=/path/to/skia
cmake --build build

# Run
./build/DropAndDrag
```

#### Build options

| Option | Default | Description |
|---|---|---|
| `DROPANDDRAG_BUILD_TESTS` | `ON` | Build unit tests |
| `DROPANDDRAG_ENABLE_SANITIZERS` | `ON` | Enable ASan/UBSan in debug |
| `DROPANDDRAG_ENABLE_LTO` | `ON` | Link-time optimization in release |
| `SKIA_DIR` | — | Path to Skia installation |

### Building Skia

Skia is required for GPU-accelerated rendering. Build it once:

```bash
git clone https://skia.googlesource.com/skia.git
cd skia
python3 tools/git-sync-deps

# macOS (Metal backend)
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

# Windows (Direct3D backend)
bin/gn gen out/Release --args='
  is_official_build=true
  skia_use_direct3d=true
  skia_use_system_libjpeg_turbo=false
  skia_use_system_libwebp=false
  skia_use_system_libpng=false
  skia_use_system_icu=false
  skia_use_system_harfbuzz=false
  skia_use_system_expat=false
  skia_enable_skottie=false
  skia_enable_svg=false
  skia_enable_pdf=false
  target_cpu="x64"
'
ninja -C out/Release

# Linux (Vulkan backend)
bin/gn gen out/Release --args='
  is_official_build=true
  skia_use_vulkan=true
  skia_use_system_libjpeg_turbo=false
  skia_use_system_libwebp=false
  skia_use_system_libpng=false
  skia_use_system_icu=false
  skia_use_system_harfbuzz=false
  skia_use_system_expat=false
  skia_enable_skottie=false
  skia_enable_svg=false
  skia_enable_pdf=false
'
ninja -C out/Release
```

Set `SKIA_DIR` to the skia checkout root when building DropAndDrag:
```bash
cmake -B build -DSKIA_DIR=/path/to/skia
```

## Packaging

### macOS DMG

```bash
./packaging/macos/create_dmg.sh
```

### Windows MSI

```batch
packaging\windows\build.bat
```

### Linux AppImage / DEB / RPM

```bash
./packaging/linux/build_appimage.sh
# or
dpkg-deb --build packaging/linux dropanddrag.deb
# or
rpmbuild -bb packaging/linux/dropanddrag.spec
```

## Architecture

```
src/
├── core/              # Platform-independent: Item, Database, Search, Collections
├── platform/          # Abstractions: Window, Drag&Drop, Clipboard, Hotkeys, Tray
├── ui/                # Skia renderer, Shelf, Item views, Themes, Animations
├── platform_impl/     # Native glue: Cocoa (.mm), Win32 (.cpp), X11 (.cpp)
└── app/               # Application lifecycle, event loop, signal handling
```

- **UI Thread** — Skia rendering, vsync-locked at display refresh rate
- **DB Thread** — SQLite3 with WAL mode, async via `std::future`, prepared statements
- **Worker Pool** — thumbnail generation, file I/O, search indexing
- **File Monitor Thread** — `FSEvents` / `ReadDirectoryChangesW` / `inotify`
- **Event Bus** — pub/sub for decoupled component communication
- **Zero-copy** where possible — move semantics, `string_view`, `span`, memory mapping

## Performance targets

| Metric | Target |
|---|---|
| Cold start | &lt;50 ms |
| Warm start | &lt;10 ms |
| Idle memory | &lt;15 MB |
| CPU idle | 0% |
| Drag response | &lt;8 ms |
| Search response | &lt;10 ms |
| Thumbnail load | async, non-blocking |

## Why C++23?

No V8, no JIT, no garbage collector. Just native code compiled to machine instructions. The entire binary is a few megabytes. It starts instantly and sips memory. Every allocation is explicit and every byte is accounted for.

DropAndDrag is engineered to be the fastest shelf utility on any platform.

## License

MIT © [DropAndDrag contributors](https://github.com/anomalyco/DropAndDrag/graphs/contributors)

---

<p align="center">
  <sub>Built with C++23 · Skia · CMake · SQLite3 · ❤️</sub>
</p>
