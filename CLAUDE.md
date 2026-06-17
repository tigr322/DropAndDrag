# CLAUDE.md — DropAndDrag

A high-performance C++23 cross-platform drag-and-drop shelf utility. GPU-accelerated via Skia, native platform backends (Cocoa/Win32/X11), SQLite3 persistence.

## Build Commands

```bash
# Configure (requires Skia pre-built, see README.md)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKIA_DIR=/path/to/skia

# Build
cmake --build build

# Build + test (one-shot convenience)
./build.sh          # macOS/Linux
build.bat           # Windows

# Run tests only
cd build && ctest --output-on-failure

# Syntax-check individual headers (no Skia needed)
clang++ -std=c++23 -fsyntax-only -Isrc -Isrc/vendor src/core/items/item.hpp

# Syntax-check macOS .mm files
clang++ -std=c++23 -fsyntax-only -Isrc -Isrc/vendor -fobjc-arc src/platform_impl/macos/window_mac.mm

# Release (build + DMG + GitHub release)
export GITHUB_TOKEN=...
export SKIA_DIR=~/skia
./scripts/release.sh 1.0.0
```

## Architecture

```
src/
├── core/                 # Platform-independent logic
│   ├── items/            #   Item model, ItemStore (thread-safe singleton)
│   ├── database/         #   SQLite3 via sqlite3.h, WAL, async via std::future
│   ├── search/           #   FTS5 full-text search engine
│   ├── collections/      #   Collection CRUD
│   ├── tags/             #   Tag CRUD + item-tag mapping
│   ├── settings/         #   JSON settings persistence
│   ├── event_bus/        #   Pub/sub event system
│   ├── threading/        #   ThreadPool (std::jthread workers)
│   └── mouse_shake/      #   MouseShakeDetector algorithm
├── platform/             # Abstract interfaces (pure virtual, no impl)
│   ├── window/           #   NativeWindow, WindowManager
│   ├── drag_drop/        #   DragDropManager, DropItemData
│   ├── clipboard/        #   ClipboardManager
│   ├── hotkeys/          #   HotkeyManager
│   ├── tray/             #   SystemTray
│   ├── fs_monitor/       #   FileSystemMonitor
│   ├── notifications/    #   Notifications
│   └── mouse_monitor/    #   start/stop_mouse_monitor (free functions)
├── ui/                   # Skia rendering layer
│   ├── renderer/         #   SkiaContext (Metal/D3D/Vulkan), Renderer
│   ├── shelf/            #   ShelfView — main floating shelf
│   ├── item_view/        #   ItemView — single item rendering
│   ├── context_menu/     #   ContextMenu — right-click menu
│   ├── search_bar/       #   SearchBar — inline search + type filters
│   ├── themes/           #   Theme — light/dark/auto color palettes
│   ├── animations/       #   Animation engine (fade, slide, spring)
│   ├── layout/           #   LayoutEngine — flow/grid, virtual scroll
│   └── components/       #   Base Component class (tree, events)
├── platform_impl/        # Concrete native implementations
│   ├── macos/            #   .mm files — Cocoa/AppKit, Metal
│   ├── windows/          #   .cpp files — Win32, Direct3D
│   └── linux/            #   .cpp files — X11/Xdnd, inotify, XInput2
├── app/                  # Application lifecycle orchestration
│   └── application.{hpp,cpp}
├── vendor/nlohmann/      # nlohmann/json.hpp (single-header, ~26k lines)
└── main.cpp              # Entry point
```

**Layering rule:** `core/` never includes `platform/` or `ui/`. `platform/` never includes `ui/`. `app/` orchestrates all three.

## Threading Model

| Thread | Role | Key constraint |
|--------|------|---------------|
| Main/UI | Event loop + Skia rendering | Never blocks on I/O |
| DB thread | SQLite3 queries via task queue | Only thread touching sqlite3* |
| Worker pool | Thumbnails, file I/O, search indexing | std::jthread, configurable count |
| File monitor | FSEvents/ReadDirectoryChangesW/inotify | Platform-specific thread |
| Mouse monitor | Global mouse hook for shake detection | Platform-specific, lightweight callback |

## Naming Conventions

- **Classes:** PascalCase — `MouseShakeDetector`, `NativeWindow`, `SkiaContext`
- **Methods:** camelCase (abstracts) or snake_case (platform internals) — `setAlwaysOnTop()`, `register_hotkey()`
- **Files:** snake_case — `mouse_shake_detector.hpp`, `window_manager.cpp`
- **Namespace:** `dd` for all project code
- **Include paths:** project-relative from `src/` — `#include <core/items/item.hpp>`

## Key Patterns

### Singleton / Registry
All manager classes use Meyer's singleton via `static T& instance()`. No global mutable state.

### Async Database
`Database::insertItem()` returns `std::future<bool>`. Internally enqueues to DB thread message queue. Never call DB methods from DB thread (deadlock).

### Event Bus
Pub/sub with token-based subscription. Events: `ItemAdded`, `ItemRemoved`, `SettingsChanged`, `ShelfShown`, etc. Components communicate through events, not direct calls.

### Platform Polymorphism
Abstract classes in `platform/` define the interface. `app/` uses `NativeWindow::create(style)` factory. Each `platform_impl/` platform registers at link time.

### nlohmann/json ADL
`to_json`/`from_json` for `dd::` types must be in `namespace dd` (not a nested namespace). ADL requires same-namespace visibility. `time_point` serialized as `int64_t` milliseconds.

### Objective-C++
`.mm` files mix C++ and ObjC. `@interface`/`@implementation` blocks must be at **global scope**, not inside `namespace dd {}`. Only the C++ functions referencing them go inside the namespace.

## Compiler Compatibility Notes

- **Apple Clang 21** does not support `std::move_only_function` — use `std::function`
- **C++23** features used: `std::jthread`, `std::stop_token`, `std::source_location`, `string_view`/`span`
- `#pragma once` for all headers (no include guards)
- macOS frameworks: Cocoa, Carbon, QuartzCore, Metal, MetalKit, UserNotifications, ApplicationServices
- Windows: dwmapi, shell32, ole32, gdi32, user32
- Linux: X11, XInput2, inotify

## Testing

- Framework: `tests/test_framework.hpp` — custom minimal, C++23 `std::source_location`
- Macros: `TEST_CASE(name)`, `ASSERT_TRUE/ASSERT_FALSE`, `ASSERT_EQ/NE/GE/GT/LE/LT`
- No external test library dependency
- `test_db.cpp` requires SQLite3 link, others are header-only
- Run: `cmake --build build && cd build && ctest`

## Code Style

- No comments unless logic is non-obvious (project rule)
- `const`-correct everywhere
- `noexcept` on trivial getters
- `[[nodiscard]]` on all pure observers
- RAII, move semantics, smart pointers exclusively
- No raw `new`/`delete` anywhere
- Prefer stack allocation; heap only via `std::unique_ptr`/`std::make_unique`
- `= delete` copy/move for all singleton/manager classes
