#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

enum class WindowStyle : uint8_t {
    Normal,
    Frameless,
    Transparent,
};

enum class MouseButton : uint8_t {
    Left,
    Right,
    Middle,
    X1,
    X2,
};

using PaintCallback = std::function<void()>;
using ResizeCallback = std::function<void(int width, int height)>;
using MouseCallback = std::function<void(int x, int y, MouseButton button)>;
using MouseMoveCallback = std::function<void(int x, int y)>;
using KeyCallback = std::function<void(int key_code, int modifiers)>;
using WindowDragEnterCallback = std::function<void(int x, int y)>;
using WindowDragOverCallback  = std::function<void(int x, int y)>;
using WindowDragLeaveCallback = std::function<void()>;
using WindowDropCallback = std::function<void(std::vector<std::string> paths)>;
using CloseCallback = std::function<void()>;

class NativeWindow {
public:
    virtual ~NativeWindow() = default;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;
    virtual void setBounds(int x, int y, int w, int h) = 0;
    virtual Rect getBounds() const = 0;
    virtual void setAlwaysOnTop(bool enabled) = 0;
    virtual void setTransparency(float alpha) = 0;
    virtual void setVisible(bool visible) = 0;
    virtual bool isVisible() const = 0;
    virtual void setTitle(std::string_view title) = 0;
    virtual void minimize() = 0;
    virtual void restore() = 0;
    virtual void* nativeHandle() const = 0;

    virtual void setPaintCallback(PaintCallback cb) = 0;
    virtual void setResizeCallback(ResizeCallback cb) = 0;
    virtual void setMouseDownCallback(MouseCallback cb) = 0;
    virtual void setMouseMoveCallback(MouseMoveCallback cb) = 0;
    virtual void setMouseUpCallback(MouseCallback cb) = 0;
    virtual void setKeyDownCallback(KeyCallback cb) = 0;
    virtual void setKeyUpCallback(KeyCallback cb) = 0;
    virtual void setDragEnterCallback(WindowDragEnterCallback cb) = 0;
    virtual void setDragOverCallback(WindowDragOverCallback cb) = 0;
    virtual void setDragLeaveCallback(WindowDragLeaveCallback cb) = 0;
    virtual void setDropCallback(WindowDropCallback cb) = 0;
    virtual void setCloseCallback(CloseCallback cb) = 0;

    // Position the window so it appears near the current cursor position.
    // Default no-op keeps non-macOS platforms compiling without changes.
    virtual void positionNearCursor(int w, int h) { (void)w; (void)h; }

    static std::unique_ptr<NativeWindow> create(WindowStyle style);
};

} // namespace dd
