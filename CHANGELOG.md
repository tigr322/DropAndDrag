# Changelog

All notable changes to DropAndDrag will be documented in this file.

## [1.0.0] — Unreleased

### Added
- Initial release
- Floating always-on-top shelf window with glassmorphism effect
- Drag & drop support for files, folders, images, text, and URLs
- Native platform backends: Cocoa (macOS), Win32 (Windows), X11 (Linux)
- GPU-accelerated rendering via Skia (Metal, Direct3D, Vulkan)
- SQLite3 persistent storage with WAL mode and FTS5 full-text search
- Multiple collections (Work, Personal, Temporary, custom)
- Color tags with filtering
- Favorites support
- Global hotkey to toggle shelf visibility
- System tray integration
- Auto-hide and transparency controls
- Dark and light themes with automatic system detection
- Configurable history retention (1 hour to indefinite)
- Instant search (&lt;10 ms response target)
- Virtual scrolling for large item lists
- Async thumbnail generation via thread pool
- Native file system monitoring (FSEvents, ReadDirectoryChangesW, inotify)
- Quick Paste via hotkey (paste last item)
- Zero telemetry, zero cloud, fully local
- Cross-platform build system (CMake 3.25+, C++23)
- Packaging: DMG (macOS), MSI (Windows), AppImage/DEB/RPM (Linux)
