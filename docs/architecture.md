# DropAndDrag Architecture

## System Overview

DropAndDrag is a fast, cross-platform drag-and-drop shelf utility built in C++23. It provides an always-on-top floating window that accepts drag-and-drop of files, text, URLs, and images, with full-text search, collections, and system tray integration.

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Entry                       │
│                        src/main.cpp                         │
├─────────────────────────────────────────────────────────────┤
│                    Application Lifecycle                     │
│                  src/app/application.cpp                    │
├──────────┬──────────────┬──────────────┬────────────────────┤
│   Core   │   Platform   │     UI       │  External Deps     │
│ Systems  │  Abstraction │  Rendering   │                    │
├──────────┼──────────────┼──────────────┼────────────────────┤
│ Database │ NativeWindow │ SkiaContext   │  SQLite  Skia     │
│ Settings │ WindowMgr   │ Renderer      │  nlohmann/json    │
│ EventBus │ DragDrop    │ Theme         │  Threads           │
│ Items    │ Clipboard   │ Animation     │                    │
│ Search   │ Hotkeys     │ ShelfView     │                    │
│ Tags     │ Tray        │ SearchBar     │                    │
│ Colls    │ FSMonitor   │ ContextMenu   │                    │
│ ThreadP  │ Notify      │               │                    │
└──────────┴──────────────┴──────────────┴────────────────────┘
```

## Component Diagram

```
main.cpp
  └─ Application::init()
       ├─ parse_args()          # Handle --hidden, --config
       ├─ init_directories()    # Create ~/Library/.../DropAndDrag/
       ├─ init_logging()        # File + console logger
       ├─ init_database()       # SQLite with WAL mode
       ├─ init_core_systems()   # EventBus, Settings, SearchEngine
       ├─ init_threading()      # ThreadPool for async work
       ├─ init_platform()       # NativeWindow, Tray, Hotkeys, etc.
       ├─ init_ui()             # SkiaContext, Renderer, Theme
       ├─ wire_event_bus()      # Connect component subscriptions
       └─ create_tray()         # System tray icon + menu

Application::run()
  ├─ [macOS  ] Cocoa run loop equivalent
  ├─ [Windows] GetMessage/DispatchMessage pump
  └─ [Linux  ] Poll-based event loop
```

## Core Layer (`src/core/`)

### Database (`db.hpp` / `db.cpp`)
- SQLite with WAL mode for concurrent reads
- Single dedicated DB thread processes all queries via a task queue
- All public methods return `std::future<T>` for async access
- FTS5 virtual table for full-text search
- JSON columns for flexible metadata storage
- Thread-safe, singleton pattern

### Item System (`item.hpp`, `item_store.hpp`, `item_store.cpp`)
- `ItemData`: UUID, type, path, content, metadata fields
- `ItemMetadata`: timestamps, favorites, tags, collection assignment
- `ItemStore`: thread-safe in-memory item cache with observer pattern
- Uses `std::shared_mutex` for concurrent read/write access
- JSON serialization via nlohmann/json

### Event Bus (`event_bus.hpp`, `event_bus.cpp`)
- Type-safe event system with subscription tokens
- Support for unsubscribe-during-emission
- Thread-safe registration and emission
- Event types: ItemAdded, ItemRemoved, ItemUpdated, SettingsChanged, ShelfShown, etc.

### Threading (`thread_pool.hpp`, `thread_pool.cpp`)
- Work-stealing thread pool using `std::jthread` with stop tokens
- `enqueue()` returns `std::future<T>` for result retrieval
- Supports move-only callables via `std::move_only_function`
- Automatic shutdown on destruction

### Settings (`settings.hpp`, `settings.cpp`)
- JSON-persisted application settings
- Individual getters/setters for each setting
- Notifies via EventBus on change
- Settings include: theme, transparency, hotkey, shelf position, etc.

## Platform Layer (`src/platform/`)

### Platform Abstraction
Each platform component has a common header interface with platform-specific implementations:

| Component          | Header       | macOS            | Windows                     | Linux                     |
|--------------------|--------------|------------------|-----------------------------|---------------------------|
| NativeWindow       | native_window.hpp | NSWindow      | CreateWindowEx + DWM        | X11/XWayland              |
| WindowManager      | window_manager.hpp | NSApplication | WinMain message pump        | Event loop                |
| DragDropManager    | drag_drop.hpp  | NSDraggingInfo  | OLE IDataObject/IDropTarget | Xdnd / Wayland protocol   |
| ClipboardManager   | clipboard.hpp  | NSPasteboard    | OLE clipboard API           | X11 selections            |
| HotkeyManager      | hotkeys.hpp    | CGEvent / Carbon | RegisterHotKey             | X11 XGrabKey              |
| SystemTray         | tray.hpp       | NSStatusBar     | Shell_NotifyIcon            | StatusNotifier (D-Bus)    |
| FileSystemMonitor  | fs_monitor.hpp | FSEvents        | ReadDirectoryChangesW       | inotify                   |
| Notifications      | notifications.hpp | User Notifications | Windows notifications   | libnotify (D-Bus)         |

## UI Layer (`src/ui/`)

### Rendering Pipeline
```
Frame Start
  ├─ SkiaContext::beginFrame()
  │    └─ Acquire next drawable from GPU swapchain
  ├─ Renderer::render(dt)
  │    ├─ Update animations (AnimationManager::updateAll)
  │    ├─ Draw background (Theme::drawBackground)
  │    ├─ Draw shelf view (ShelfView)
  │    ├─ Draw search bar (SearchBar)
  │    ├─ Draw context menu (ContextMenu, if visible)
  │    └─ Draw drag indicator (if active)
  └─ SkiaContext::endFrame()
       └─ Present drawable / flush GPU commands
```

### GPU Backend Selection
- **macOS**: Metal via `GrMtlBackendContext`
- **Windows**: Direct3D 11/12 via `GrD3DBackendContext`
- **Linux**: Vulkan via `GrVkBackendContext`

### Animation System
- `Animation` base class with progress tracking
- `Easing` functions: Linear, EaseIn, EaseOut, EaseInOut, Spring
- Specializations: `FadeAnimation`, `SlideAnimation`, `ScaleAnimation`, `BounceAnimation`
- `AnimationManager` owns and updates all active animations

### Theme System
- Light/Dark variants with system theme detection
- `ThemePalette` defines all UI colors
- Smooth theme transitions
- `getColor(name)` for named color lookup

## Threading Model

```
┌───────────────────────────────┐
│          UI Thread            │
│  (Main thread)                │
│  - Event loop (macOS/Windows) │
│  - Rendering frame callbacks  │
│  - User input handling        │
│  - System tray callbacks      │
└──────────────┬────────────────┘
               │
   ┌───────────┴───────────┐
   │                       │
┌──▼──────────┐   ┌────────▼──────────┐
│  DB Thread   │   │  ThreadPool       │
│  (dedicated) │   │  (N workers)      │
│              │   │                   │
│  - All SQL   │   │  - Thumbnail gen  │
│  - Migrations│   │  - Icon extraction│
│  - WAL writes│   │  - File scanning  │
│  - FTS search│   │  - Async tasks    │
└──────────────┘   └───────────────────┘
                           │
               ┌───────────▼───────────┐
               │   FileSystemMonitor   │
               │   (dedicated thread)  │
               │                       │
               │  - Directory watching │
               │  - File change events │
               └───────────────────────┘
```

## Data Flow

### Adding an Item via Drag-and-Drop
1. User drops file/text/URL onto shelf window
2. Platform DragDropManager receives drop event
3. Event emitted: `EventType::Drop` with drop data
4. EventBus delivers to ItemStore observer
5. ItemStore creates Item with UUID, metadata
6. ItemStore emits `StoreEvent::Added`
7. Database async insert via DB thread
8. EventBus emits `EventType::ItemAdded`
9. ShelfView re-renders with new item
10. Renderer marks dirty, triggers next frame

### Search Flow
1. User types in search bar
2. EventBus emits `EventType::SearchQueryChanged`
3. SearchEngine tokenizes query
4. Database FTS5 search returns matching UUIDs
5. ItemStore filters in-memory items
6. ShelfView re-renders filtered results
7. Results updated in real-time with prefix matching

## Event System Design

- **Pub-Sub pattern** via `EventBus` singleton
- Subscriptions identified by opaque `SubscriptionToken`
- Events carry type (`EventType` enum) + JSON data payload
- Thread-safe unsubscribe, including during emission
- Used for cross-layer communication without direct coupling

```
Platform Layer ──(events)──► Core Layer ──(events)──► UI Layer
       ▲                      │   ▲                      │
       │                      ▼   │                      ▼
       └────────────(events)──┘   └──────────(events)────┘
```

## Application Lifecycle

1. **Startup**
   - Parse CLI args
   - Create app data directory
   - Init logging
   - Init database + run migrations
   - Init core systems (EventBus, Settings, SearchEngine)
   - Init ThreadPool
   - Init platform components
   - Init UI (Skia, Renderer, Theme, Animations)
   - Wire event bus subscriptions
   - Register global hotkey
   - Create system tray
   - Optionally show shelf

2. **Runtime**
   - Main event loop processes platform events
   - Render frames at target FPS
   - Handle drag-and-drop, keyboard, mouse input
   - Process file system change notifications
   - Execute async tasks on thread pool

3. **Shutdown**
   - Signal received (SIGINT/SIGTERM) or Close action
   - Stop thread pool
   - Shutdown renderer
   - Close database (flush WAL checkpoint)
   - Cleanup signal handlers
   - Return EXIT_SUCCESS

## Platform-Specific Implementation Details

### macOS
- Objective-C++ `.mm` files for Cocoa integration
- Metal GPU backend via Skia
- Sandbox-aware file access
- LSUIElement for dockless operation

### Windows
- Win32 API for windowing (no Qt/Electron dependency)
- Direct3D GPU backend via Skia
- MSI installer via WiX Toolset
- Dark mode support via `DwmSetWindowAttribute`

### Linux
- X11/XWayland windowing
- Vulkan GPU backend via Skia
- Multiple packaging: AppImage, .deb, .rpm
- Desktop file for application menu integration
