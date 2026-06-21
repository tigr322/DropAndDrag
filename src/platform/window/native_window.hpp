#pragma once

// native_window.hpp — Abstract window interface.
//
// Each platform provides a concrete subclass registered at link time.
// The shelf uses WindowStyle::Transparent — a borderless, always-on-top,
// non-activating floating panel (NSPanel on macOS, layered HWND on Windows).
//
// All callbacks are invoked on the main / UI thread.
// Callback setters are safe to call before or after show(); they take effect
// immediately because the concrete impl re-wires them on every setDropCallback
// call (see DDMacWindow::setDropCallback for the historical reason).
//
// Factory: NativeWindow::create(style) returns the platform implementation.
// The Application owns the returned unique_ptr.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

// Screen rectangle in physical pixels (top-left origin on all platforms).
struct Rect {
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;
};

// Controls how the window is decorated and composited.
enum class WindowStyle : uint8_t {
    Normal,       // standard title bar + resize handles
    Frameless,    // no title bar; custom chrome
    Transparent,  // frameless + transparent background (used for the shelf)
};

// Mouse button indices match NSEvent::buttonNumber on macOS.
enum class MouseButton : uint8_t {
    Left,
    Right,
    Middle,
    X1,
    X2,
};

// --- Callback types ---
using PaintCallback           = std::function<void()>;
using ResizeCallback          = std::function<void(int width, int height)>;
using MouseCallback           = std::function<void(int x, int y, MouseButton button)>;
using MouseMoveCallback       = std::function<void(int x, int y)>;
using KeyCallback             = std::function<void(int key_code, int modifiers)>;
using WindowDragEnterCallback = std::function<void(int x, int y)>;
using WindowDragOverCallback  = std::function<void(int x, int y)>;
using WindowDragLeaveCallback = std::function<void()>;
using WindowDropCallback      = std::function<void(std::vector<std::string> paths)>;
using CloseCallback           = std::function<void()>;

class NativeWindow {
public:
    virtual ~NativeWindow() = default;

    // --- Visibility ---
    virtual void show()  = 0;   // macOS: orderFrontRegardless (works while app is background)
    virtual void hide()  = 0;   // macOS: orderOut:nil
    virtual void close() = 0;   // macOS: [NSWindow close]

    // --- Geometry ---
    virtual void setBounds(int x, int y, int w, int h) = 0;  // origin is bottom-left on macOS
    [[nodiscard]] virtual Rect getBounds() const = 0;

    // --- Window properties ---
    virtual void setAlwaysOnTop(bool enabled) = 0;   // macOS: NSFloatingWindowLevel
    virtual void setTransparency(float alpha) = 0;   // 0.0 = invisible, 1.0 = opaque
    virtual void setVisible(bool visible) = 0;       // convenience wrapper for show/hide
    [[nodiscard]] virtual bool isVisible() const = 0;
    virtual void setTitle(std::string_view title) = 0;
    virtual void minimize() = 0;
    virtual void restore()  = 0;

    // Returns the native view handle for the renderer to draw into.
    // macOS: returns (__bridge void*)contentView (NSView*)
    [[nodiscard]] virtual void* nativeHandle() const = 0;

    // --- Event callbacks ---
    // All callbacks fire on the main thread.
    virtual void setPaintCallback     (PaintCallback cb)           = 0;
    virtual void setResizeCallback    (ResizeCallback cb)          = 0;
    virtual void setMouseDownCallback (MouseCallback cb)           = 0;
    virtual void setMouseMoveCallback (MouseMoveCallback cb)       = 0;
    virtual void setMouseUpCallback   (MouseCallback cb)           = 0;
    virtual void setKeyDownCallback   (KeyCallback cb)             = 0;
    virtual void setKeyUpCallback     (KeyCallback cb)             = 0;
    virtual void setDragEnterCallback (WindowDragEnterCallback cb) = 0;
    virtual void setDragOverCallback  (WindowDragOverCallback cb)  = 0;
    virtual void setDragLeaveCallback (WindowDragLeaveCallback cb) = 0;
    virtual void setDropCallback      (WindowDropCallback cb)      = 0;
    virtual void setCloseCallback     (CloseCallback cb)           = 0;

    // Move the window so its centre is near the current cursor position.
    // Default is a no-op so non-macOS platforms compile without changes.
    virtual void positionNearCursor(int w, int h) { (void)w; (void)h; }

    // Pump pending platform events. Called each tick on Linux; no-op elsewhere.
    virtual void processEvents() {}

    // Platform factory — links against the appropriate platform_impl/ module.
    static std::unique_ptr<NativeWindow> create(WindowStyle style);
};

} // namespace dd
