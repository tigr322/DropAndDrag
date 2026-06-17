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
│   ├── items/            #   Item model, IItemStore interface, ItemStore impl
│   │                     #   item_json.hpp — nlohmann serialization (separate from item.hpp)
│   ├── database/         #   SQLite3 via sqlite3.h, WAL, async via std::future
│   │                     #   migrations.cpp — versioned, each migration in its own transaction
│   ├── search/           #   FTS5 full-text search engine
│   ├── collections/      #   Collection struct; collection_json.hpp for serialization
│   ├── tags/             #   Tag struct; tag_json.hpp for serialization
│   ├── settings/         #   JSON settings persistence
│   ├── event_bus/        #   Pub/sub event system (Event::data is std::any, no json dep)
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

### Singleton / DI
Core services (`ItemStore`, `EventBus`) expose a `static T& instance()` for backward compatibility. `Application` owns the concrete instances as `unique_ptr` and wires them via `set_instance_for_test()` on startup (and clears on shutdown). This means:
- Production: all callers go through the Application-owned instance — no hidden second object.
- Tests: call `ItemStore::set_instance_for_test(&mock)` before the code under test, `nullptr` after.
- `Database` has no singleton — `Application` creates it directly and passes it where needed.

### Testability interfaces
`IItemStore` (`core/items/iitem_store.hpp`) is a pure-virtual interface that `ItemStore` implements. Write mock stores by inheriting from `IItemStore`. `EventBus` is directly constructible for test isolation.

### Async Database
`Database::insertItem()` returns `std::future<bool>`. Internally enqueues to DB thread message queue. Never call DB methods from DB thread (deadlock). `Database` constructor is public; ownership lives in `Application`.

### Event Bus
Pub/sub with token-based subscription. `Event::data` is `std::any` — producers store typed values, subscribers `std::any_cast<T>(e.data)`. Events: `ItemAdded`, `ItemRemoved`, `SettingsChanged`, `ShelfShown`, etc. Components communicate through events, not direct calls.

### Platform Polymorphism
Abstract classes in `platform/` define the interface. `app/` uses `NativeWindow::create(style)` factory. Each `platform_impl/` platform registers at link time.

### nlohmann/json isolation
Core structs (`Item`, `Collection`, `Tag`) live in json-free headers. Serialization lives in companion `*_json.hpp` headers that include `<vendor/nlohmann/json.hpp>`. Only include the companion when you actually need to convert to/from JSON. `to_json`/`from_json` must remain in `namespace dd` for ADL. `time_point` serialized as `int64_t` milliseconds.

### Migration safety
Each `migrate_vN()` runs inside `BEGIN IMMEDIATE / COMMIT / ROLLBACK`. A partial failure leaves the DB unchanged (version number not bumped); the next startup retries.

### Objective-C++
`.mm` files mix C++ and ObjC. `@interface`/`@implementation` blocks must be at **global scope**, not inside `namespace dd {}`. Only the C++ functions referencing them go inside the namespace.

## Compiler Compatibility Notes

- **Apple Clang 21** does not support `std::move_only_function` — use `std::function` everywhere (task queues, callbacks). The codebase enforces this.
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
