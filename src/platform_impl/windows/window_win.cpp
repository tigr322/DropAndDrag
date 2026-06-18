// window_win.cpp — Windows NativeWindow stub (Win32 WS_EX_LAYERED implementation pending).

#include "platform/window/native_window.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <algorithm>
#include <cassert>
#include <cwctype>
#include <string>
#include <string_view>

namespace dd {
namespace win {
namespace {

std::wstring toWide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

std::string toUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len, nullptr, nullptr);
    return result;
}

UINT mapMouseButton(WPARAM wp) {
    switch (wp) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:       return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:       return 1;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:       return 2;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:       return GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? 3 : 4;
    default:                 return 0;
    }
}

int mapVKeyToKeyCode(WPARAM vk) {
    return static_cast<int>(vk);
}

int mapModifiers() {
    int mods = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)    mods |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000)   mods |= MOD_SHIFT;
    if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) mods |= MOD_WIN;
    return mods;
}

} // namespace
} // namespace win

class DDWinWindow : public NativeWindow {
public:
    explicit DDWinWindow(WindowStyle style) : style_(style) {
        registerWindowClass();
        DWORD ex_style = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_APPWINDOW;
        DWORD style_flags = WS_POPUP;
        if (style == WindowStyle::Normal) {
            ex_style = WS_EX_APPWINDOW;
            style_flags = WS_OVERLAPPEDWINDOW;
        } else if (style == WindowStyle::Frameless) {
            ex_style = WS_EX_APPWINDOW;
            style_flags = WS_POPUP | WS_THICKFRAME;
        }
        hwnd_ = CreateWindowExW(
            ex_style,
            L"DropAndDragShelf",
            L"DropAndDrag",
            style_flags,
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 80,
            nullptr, nullptr, GetModuleHandleW(nullptr), this);
        assert(hwnd_ && "Failed to create window");
        dpi_ = GetDpiForWindow(hwnd_);
        if (style == WindowStyle::Transparent) {
            enableBlur();
        }
    }

    ~DDWinWindow() override {
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    HWND getHWND() const { return hwnd_; }

    void show() override {
        ShowWindow(hwnd_, SW_SHOW);
        visible_ = true;
    }

    void hide() override {
        ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
    }

    void close() override {
        if (hwnd_) {
            if (closeCallback_) closeCallback_();
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    void setBounds(int x, int y, int w, int h) override {
        SetWindowPos(hwnd_, nullptr, x, y, w, h,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }

    Rect getBounds() const override {
        RECT r{};
        GetWindowRect(hwnd_, &r);
        return {r.left, r.top, r.right - r.left, r.bottom - r.top};
    }

    void setAlwaysOnTop(bool enabled) override {
        always_on_top_ = enabled;
        HWND insert_after = enabled ? HWND_TOPMOST : HWND_NOTOPMOST;
        SetWindowPos(hwnd_, insert_after, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    void setTransparency(float alpha) override {
        transparency_ = std::clamp(alpha, 0.0f, 1.0f);
        BYTE b_alpha = static_cast<BYTE>(transparency_ * 255.0f);
        if (style_ == WindowStyle::Transparent) {
            SetLayeredWindowAttributes(hwnd_, 0, b_alpha, LWA_ALPHA);
        }
    }

    void setVisible(bool visible) override {
        visible_ = visible;
        ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
    }

    bool isVisible() const override {
        return visible_ && IsWindowVisible(hwnd_);
    }

    void setTitle(std::string_view title) override {
        SetWindowTextW(hwnd_, win::toWide(title).c_str());
    }

    void minimize() override {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }

    void restore() override {
        ShowWindow(hwnd_, SW_RESTORE);
    }

    void setPaintCallback(PaintCallback cb) override        { paintCallback_ = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override       { resizeCallback_ = std::move(cb); }
    void setMouseDownCallback(MouseCallback cb) override     { mouseDownCallback_ = std::move(cb); }
    void setMouseMoveCallback(MouseMoveCallback cb) override { mouseMoveCallback_ = std::move(cb); }
    void setMouseUpCallback(MouseCallback cb) override       { mouseUpCallback_ = std::move(cb); }
    void setKeyDownCallback(KeyCallback cb) override         { keyDownCallback_ = std::move(cb); }
    void setKeyUpCallback(KeyCallback cb) override           { keyUpCallback_ = std::move(cb); }
    void setDragEnterCallback(WindowDragEnterCallback cb) override { dragEnterCallback_ = std::move(cb); }
    void setDragOverCallback(WindowDragOverCallback cb) override   { dragOverCallback_ = std::move(cb); }
    void setDragLeaveCallback(WindowDragLeaveCallback cb) override { dragLeaveCallback_ = std::move(cb); }
    void setDropCallback(WindowDropCallback cb) override           { dropCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) override         { closeCallback_ = std::move(cb); }

private:
    void enableBlur() {
        DWM_BLURBEHIND bb{};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
        DwmEnableBlurBehindWindow(hwnd_, &bb);
        if (bb.hRgnBlur) DeleteObject(bb.hRgnBlur);

        MARGINS m{-1};
        DwmExtendFrameIntoClientArea(hwnd_, &m);
    }

    static void registerWindowClass() {
        static bool registered = false;
        if (registered) return;
        registered = true;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &DDWinWindow::wndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = L"DropAndDragShelf";
        RegisterClassExW(&wc);
    }

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            auto* self = static_cast<DDWinWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        auto* self = reinterpret_cast<DDWinWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self) {
            return self->handleMessage(msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            if (paintCallback_) paintCallback_();
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_SIZE: {
            int w = LOWORD(lp);
            int h = HIWORD(lp);
            if (resizeCallback_) resizeCallback_(w, h);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            if (mouseDownCallback_) mouseDownCallback_(x, y, MouseButton::Left);
            return 0;
        }
        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            if (mouseUpCallback_) mouseUpCallback_(x, y, MouseButton::Left);
            return 0;
        }
        case WM_RBUTTONDOWN: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            if (mouseDownCallback_) mouseDownCallback_(x, y, MouseButton::Right);
            return 0;
        }
        case WM_RBUTTONUP: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            if (mouseUpCallback_) mouseUpCallback_(x, y, MouseButton::Right);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            if (mouseMoveCallback_) mouseMoveCallback_(x, y);
            return 0;
        }
        case WM_KEYDOWN: {
            int vk = static_cast<int>(wp);
            int mods = win::mapModifiers();
            if (keyDownCallback_) keyDownCallback_(vk, mods);
            return 0;
        }
        case WM_KEYUP: {
            int vk = static_cast<int>(wp);
            int mods = win::mapModifiers();
            if (keyUpCallback_) keyUpCallback_(vk, mods);
            return 0;
        }
        case WM_CLOSE: {
            if (closeCallback_) closeCallback_();
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wp);
            auto* rect = reinterpret_cast<RECT*>(lp);
            SetWindowPos(hwnd_, nullptr, rect->left, rect->top,
                         rect->right - rect->left, rect->bottom - rect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_NCHITTEST: {
            if (style_ != WindowStyle::Normal) {
                POINT pt{LOWORD(lp), HIWORD(lp)};
                ScreenToClient(hwnd_, &pt);
                RECT rc{};
                GetClientRect(hwnd_, &rc);
                if (pt.y < 8) return HTCAPTION;
            }
            return DefWindowProcW(hwnd_, msg, wp, lp);
        }
        default:
            return DefWindowProcW(hwnd_, msg, wp, lp);
        }
    }

    HWND hwnd_ = nullptr;
    UINT dpi_ = 96;
    bool visible_ = false;
    WindowStyle style_;
    float transparency_ = 1.0f;
    bool always_on_top_ = true;

    PaintCallback paintCallback_;
    ResizeCallback resizeCallback_;
    MouseCallback mouseDownCallback_;
    MouseMoveCallback mouseMoveCallback_;
    MouseCallback mouseUpCallback_;
    KeyCallback keyDownCallback_;
    KeyCallback keyUpCallback_;
    WindowDragEnterCallback dragEnterCallback_;
    WindowDragOverCallback dragOverCallback_;
    WindowDragLeaveCallback dragLeaveCallback_;
    WindowDropCallback dropCallback_;
    CloseCallback closeCallback_;
};

std::unique_ptr<NativeWindow> NativeWindow::create(WindowStyle style) {
    return std::make_unique<DDWinWindow>(style);
}

} // namespace dd
