#include "platform/window/native_window.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace dd {

namespace {

std::mutex g_display_mutex;
Display* g_display = nullptr;
std::atomic<int> g_display_refcount{0};

int x11ErrorHandler(Display* /*dpy*/, XErrorEvent* /*ev*/) {
    return 0;
}

int x11IOErrorHandler(Display* /*dpy*/) {
    return 0;
}

Display* acquireDisplay() {
    std::lock_guard<std::mutex> lock(g_display_mutex);
    if (!g_display) {
        XSetErrorHandler(x11ErrorHandler);
        XSetIOErrorHandler(x11IOErrorHandler);
        g_display = XOpenDisplay(nullptr);
        if (!g_display) {
            throw std::runtime_error("DDLinuxWindow: failed to open X11 display");
        }
    }
    g_display_refcount.fetch_add(1, std::memory_order_relaxed);
    return g_display;
}

void releaseDisplay() {
    std::lock_guard<std::mutex> lock(g_display_mutex);
    if (g_display_refcount.fetch_sub(1, std::memory_order_relaxed) == 1) {
        XCloseDisplay(g_display);
        g_display = nullptr;
    }
}

class AtomCache {
public:
    static AtomCache& instance() {
        static AtomCache cache;
        return cache;
    }

    Atom get(Display* dpy, const char* name) {
        auto it = atoms_.find(name);
        if (it != atoms_.end()) return it->second;
        Atom a = XInternAtom(dpy, name, False);
        atoms_[name] = a;
        return a;
    }

private:
    std::unordered_map<std::string, Atom> atoms_;
};

template<typename F>
auto withDisplay(F&& f) -> decltype(f(std::declval<Display*>())) {
    Display* dpy = acquireDisplay();
    try {
        auto result = f(dpy);
        releaseDisplay();
        return result;
    } catch (...) {
        releaseDisplay();
        throw;
    }
}

unsigned int x11Modifiers(uint8_t modifiers) {
    unsigned int result = 0;
    if (modifiers & static_cast<uint8_t>(Modifier::Ctrl))  result |= ControlMask;
    if (modifiers & static_cast<uint8_t>(Modifier::Alt))   result |= Mod1Mask;
    if (modifiers & static_cast<uint8_t>(Modifier::Shift)) result |= ShiftMask;
    if (modifiers & static_cast<uint8_t>(Modifier::Meta))  result |= Mod4Mask;
    return result;
}

int x11ButtonToMouseButton(unsigned int button) {
    switch (button) {
        case Button1: return 0;
        case Button2: return 1;
        case Button3: return 2;
        default: return button - Button1;
    }
}

MouseButton x11ButtonToEnum(unsigned int button) {
    switch (button) {
        case Button2: return MouseButton::Middle;
        case Button3: return MouseButton::Right;
        default: return MouseButton::Left;
    }
}

unsigned int mouseButtonToX11(MouseButton btn) {
    switch (btn) {
        case MouseButton::Left: return Button1Mask;
        case MouseButton::Middle: return Button2Mask;
        case MouseButton::Right: return Button3Mask;
        default: return 0;
    }
}

bool isX11Error = false;

} // anonymous namespace

class DDLinuxWindow final : public NativeWindow {
public:
    explicit DDLinuxWindow(WindowStyle style)
        : style_(style), display_(acquireDisplay())
    {
        auto& ac = AtomCache::instance();

        screen_ = DefaultScreen(display_);
        root_ = RootWindow(display_, screen_);
        visual_ = DefaultVisual(display_, screen_);
        depth_ = DefaultDepth(display_, screen_);
        colormap_ = DefaultColormap(display_, screen_);

        unsigned long attr_mask = CWEventMask | CWColormap | CWBorderPixel;
        XSetWindowAttributes attrs{};
        attrs.event_mask = ExposureMask | StructureNotifyMask | PointerMotionMask |
                           ButtonPressMask | ButtonReleaseMask | KeyPressMask |
                           KeyReleaseMask | PropertyChangeMask | SubstructureNotifyMask;
        attrs.colormap = colormap_;
        attrs.border_pixel = 0;

        if (style_ == WindowStyle::Frameless || style_ == WindowStyle::Transparent) {
            override_redirect_ = true;
        }

        int x = 0, y = 0;
        unsigned int w = 400, h = 200;

        window_ = XCreateWindow(display_, root_,
                                x, y, w, h, 0,
                                depth_, InputOutput, visual_,
                                attr_mask, &attrs);

        if (override_redirect_) {
            XSetWindowAttributes override_attrs{};
            override_attrs.override_redirect = True;
            XChangeWindowAttributes(display_, window_, CWOverrideRedirect, &override_attrs);
        }

        XStoreName(display_, window_, "DropAndDrag");

        wm_delete_window_ = ac.get(display_, "WM_DELETE_WINDOW");
        XSetWMProtocols(display_, window_, &wm_delete_window_, 1);

        wm_protocols_ = ac.get(display_, "WM_PROTOCOLS");

        utf8_string_ = ac.get(display_, "UTF8_STRING");
        net_wm_name_ = ac.get(display_, "_NET_WM_NAME");
        net_wm_state_ = ac.get(display_, "_NET_WM_STATE");
        net_wm_state_above_ = ac.get(display_, "_NET_WM_STATE_ABOVE");
        net_wm_window_type_ = ac.get(display_, "_NET_WM_WINDOW_TYPE");
        net_wm_window_type_dock_ = ac.get(display_, "_NET_WM_WINDOW_TYPE_DOCK");
        net_wm_window_type_utility_ = ac.get(display_, "_NET_WM_WINDOW_TYPE_UTILITY");
        net_wm_window_opacity_ = ac.get(display_, "_NET_WM_WINDOW_OPACITY");

        text_uri_list_ = ac.get(display_, "text/uri-list");

        setWindowType();

        XSizeHints* hints = XAllocSizeHints();
        if (hints) {
            hints->flags = PMinSize;
            hints->min_width = 100;
            hints->min_height = 50;
            XSetWMNormalHints(display_, window_, hints);
            XFree(hints);
        }

        XClassHint* class_hint = XAllocClassHint();
        if (class_hint) {
            class_hint->res_name = const_cast<char*>("dropanddrag");
            class_hint->res_class = const_cast<char*>("DropAndDrag");
            XSetClassHint(display_, window_, class_hint);
            XFree(class_hint);
        }
    }

    ~DDLinuxWindow() override {
        if (window_) {
            XDestroyWindow(display_, window_);
            XFlush(display_);
            window_ = 0;
        }
        releaseDisplay();
    }

    void show() override {
        XMapWindow(display_, window_);
        XFlush(display_);
        visible_ = true;
    }

    void hide() override {
        XUnmapWindow(display_, window_);
        XFlush(display_);
        visible_ = false;
    }

    void close() override {
        hide();
        if (close_callback_) close_callback_();
    }

    void setBounds(int x, int y, int w, int h) override {
        XMoveResizeWindow(display_, window_, x, y,
                          static_cast<unsigned int>(std::max(w, 1)),
                          static_cast<unsigned int>(std::max(h, 1)));
        XFlush(display_);
    }

    Rect getBounds() const override {
        ::Window root_ret, child_ret;
        int root_x, root_y, win_x, win_y;
        unsigned int w, h, border, depth_ret;
        XGetGeometry(display_, window_, &root_ret,
                     &win_x, &win_y, &w, &h, &border, &depth_ret);
        XTranslateCoordinates(display_, window_, root_,
                              0, 0, &root_x, &root_y, &child_ret);
        return {root_x, root_y, static_cast<int>(w), static_cast<int>(h)};
    }

    void setAlwaysOnTop(bool enabled) override {
        auto& ac = AtomCache::instance();
        Atom state_above = ac.get(display_, "_NET_WM_STATE_ABOVE");
        Atom state_hidden = ac.get(display_, "_NET_WM_STATE_HIDDEN");

        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = window_;
        ev.xclient.message_type = net_wm_state_;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = enabled ? 1 : 0;
        ev.xclient.data.l[1] = static_cast<long>(state_above);
        ev.xclient.data.l[2] = 0;
        ev.xclient.data.l[3] = 0;
        ev.xclient.data.l[4] = 0;

        XSendEvent(display_, root_, False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        XFlush(display_);
    }

    void setTransparency(float alpha) override {
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        unsigned long opacity = static_cast<unsigned long>(alpha * 0xFFFFFFFF);
        XChangeProperty(display_, window_, net_wm_window_opacity_,
                        XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&opacity), 1);
        XFlush(display_);
    }

    void setVisible(bool visible) override {
        if (visible) show(); else hide();
    }

    bool isVisible() const override {
        return visible_;
    }

    void setTitle(std::string_view title) override {
        XStoreName(display_, window_, title.data());

        auto& ac = AtomCache::instance();
        Atom net_name = ac.get(display_, "_NET_WM_NAME");
        XChangeProperty(display_, window_, net_name, utf8_string_, 8,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(title.data()),
                        static_cast<int>(title.size()));
        XFlush(display_);
    }

    void minimize() override {
        XIconifyWindow(display_, window_, screen_);
        XFlush(display_);
    }

    void restore() override {
        XMapWindow(display_, window_);
        XRaiseWindow(display_, window_);
        XFlush(display_);
        visible_ = true;
    }

    void setPaintCallback(PaintCallback cb) override { paint_callback_ = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override { resize_callback_ = std::move(cb); }
    void setMouseDownCallback(MouseCallback cb) override { mouse_down_callback_ = std::move(cb); }
    void setMouseMoveCallback(MouseMoveCallback cb) override { mouse_move_callback_ = std::move(cb); }
    void setMouseUpCallback(MouseCallback cb) override { mouse_up_callback_ = std::move(cb); }
    void setKeyDownCallback(KeyCallback cb) override { key_down_callback_ = std::move(cb); }
    void setKeyUpCallback(KeyCallback cb) override { key_up_callback_ = std::move(cb); }
    void setDragEnterCallback(WindowDragEnterCallback cb) override { drag_enter_callback_ = std::move(cb); }
    void setDragOverCallback(WindowDragOverCallback cb) override { drag_over_callback_ = std::move(cb); }
    void setDragLeaveCallback(WindowDragLeaveCallback cb) override { drag_leave_callback_ = std::move(cb); }
    void setDropCallback(WindowDropCallback cb) override { drop_callback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) override { close_callback_ = std::move(cb); }

    ::Window x11Window() const { return window_; }
    Display* display() const { return display_; }

    void processEvents() {
        while (XPending(display_)) {
            XEvent ev;
            XNextEvent(display_, &ev);
            handleEvent(ev);
        }
    }

private:
    void setWindowType() {
        Atom type = net_wm_window_type_utility_;
        XChangeProperty(display_, window_, net_wm_window_type_,
                        XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&type), 1);
    }

    void handleEvent(const XEvent& ev) {
        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0 && paint_callback_) {
                paint_callback_();
            }
            break;

        case ConfigureNotify:
            if (resize_callback_) {
                resize_callback_(ev.xconfigure.width, ev.xconfigure.height);
            }
            break;

        case MotionNotify:
            if (mouse_move_callback_) {
                mouse_move_callback_(ev.xmotion.x, ev.xmotion.y);
            }
            break;

        case ButtonPress:
            if (mouse_down_callback_) {
                mouse_down_callback_(ev.xbutton.x, ev.xbutton.y,
                                     x11ButtonToEnum(ev.xbutton.button));
            }
            break;

        case ButtonRelease:
            if (mouse_up_callback_) {
                mouse_up_callback_(ev.xbutton.x, ev.xbutton.y,
                                   x11ButtonToEnum(ev.xbutton.button));
            }
            break;

        case KeyPress:
        case KeyRelease: {
            KeySym keysym = XLookupKeysym(const_cast<XKeyEvent*>(&ev.xkey), 0);
            unsigned int state = ev.xkey.state;
            uint8_t modifiers = 0;
            if (state & ControlMask) modifiers |= static_cast<uint8_t>(Modifier::Ctrl);
            if (state & Mod1Mask)    modifiers |= static_cast<uint8_t>(Modifier::Alt);
            if (state & ShiftMask)   modifiers |= static_cast<uint8_t>(Modifier::Shift);
            if (state & Mod4Mask)    modifiers |= static_cast<uint8_t>(Modifier::Meta);

            if (ev.type == KeyPress && key_down_callback_) {
                key_down_callback_(static_cast<int>(keysym), modifiers);
            } else if (ev.type == KeyRelease && key_up_callback_) {
                key_up_callback_(static_cast<int>(keysym), modifiers);
            }
            break;
        }

        case ClientMessage: {
            auto& ac = AtomCache::instance();
            Atom wm_protos = ac.get(display_, "WM_PROTOCOLS");
            Atom wm_delete = ac.get(display_, "WM_DELETE_WINDOW");

            if (ev.xclient.message_type == wm_protos &&
                static_cast<Atom>(ev.xclient.data.l[0]) == wm_delete) {
                close();
            } else {
                handleDragDropEvent(ev);
            }
            break;
        }

        default:
            break;
        }
    }

    void handleDragDropEvent(const XEvent& ev);

    Display* display_;
    int screen_;
    ::Window root_;
    ::Window window_{0};
    Visual* visual_;
    int depth_;
    Colormap colormap_;
    WindowStyle style_;
    bool override_redirect_{false};
    bool visible_{false};

    Atom wm_delete_window_;
    Atom wm_protocols_;
    Atom utf8_string_;
    Atom net_wm_name_;
    Atom net_wm_state_;
    Atom net_wm_state_above_;
    Atom net_wm_window_type_;
    Atom net_wm_window_type_dock_;
    Atom net_wm_window_type_utility_;
    Atom net_wm_window_opacity_;
    Atom text_uri_list_;

    PaintCallback paint_callback_;
    ResizeCallback resize_callback_;
    MouseCallback mouse_down_callback_;
    MouseMoveCallback mouse_move_callback_;
    MouseCallback mouse_up_callback_;
    KeyCallback key_down_callback_;
    KeyCallback key_up_callback_;
    WindowDragEnterCallback drag_enter_callback_;
    WindowDragOverCallback drag_over_callback_;
    WindowDragLeaveCallback drag_leave_callback_;
    WindowDropCallback drop_callback_;
    CloseCallback close_callback_;
};

std::unique_ptr<NativeWindow> NativeWindow::create(WindowStyle style) {
    return std::make_unique<DDLinuxWindow>(style);
}

class XDndHandler {
public:
    static XDndHandler& instance() {
        static XDndHandler handler;
        return handler;
    }

    void handleClientMessage(DDLinuxWindow* win, const XClientMessageEvent& ev) {
        Display* dpy = win->display();
        auto& ac = AtomCache::instance();

        if (static_cast<Atom>(ev.message_type) == XdndEnter) {
            handleXdndEnter(dpy, win->x11Window(), ev);
        } else if (static_cast<Atom>(ev.message_type) == XdndPosition) {
            handleXdndPosition(dpy, win->x11Window(), ev);
        } else if (static_cast<Atom>(ev.message_type) == XdndDrop) {
            handleXdndDrop(dpy, win->x11Window(), ev);
        } else if (static_cast<Atom>(ev.message_type) == XdndLeave) {
            handleXdndLeave(dpy, win->x11Window(), ev);
        }
    }

    Atom XdndEnter;
    Atom XdndPosition;
    Atom XdndStatus;
    Atom XdndDrop;
    Atom XdndLeave;
    Atom XdndFinished;
    Atom XdndSelection;
    Atom XdndTypeList;
    Atom XdndActionCopy;
    Atom XdndActionMove;
    Atom XdndActionLink;

private:
    XDndHandler() {
        Display* dpy = acquireDisplay();
        auto& ac = AtomCache::instance();
        XdndEnter = ac.get(dpy, "XdndEnter");
        XdndPosition = ac.get(dpy, "XdndPosition");
        XdndStatus = ac.get(dpy, "XdndStatus");
        XdndDrop = ac.get(dpy, "XdndDrop");
        XdndLeave = ac.get(dpy, "XdndLeave");
        XdndFinished = ac.get(dpy, "XdndFinished");
        XdndSelection = ac.get(dpy, "XdndSelection");
        XdndTypeList = ac.get(dpy, "XdndTypeList");
        XdndActionCopy = ac.get(dpy, "XdndActionCopy");
        XdndActionMove = ac.get(dpy, "XdndActionMove");
        XdndActionLink = ac.get(dpy, "XdndActionLink");
        releaseDisplay();
    }

    void handleXdndEnter(Display* dpy, ::Window wnd, const XClientMessageEvent& ev) {
        source_window_ = static_cast<::Window>(ev.data.l[0]);
        xdnd_version_ = std::min(static_cast<unsigned long>(ev.data.l[1] >> 24), 5UL);
        has_type_list_ = (ev.data.l[1] & 1) != 0;

        accepted_types_.clear();

        if (has_type_list_ && xdnd_version_ >= 1) {
            auto& ac = AtomCache::instance();
            Atom type_list_atom = ac.get(dpy, "XdndTypeList");
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char* data = nullptr;

            if (XGetWindowProperty(dpy, source_window_, type_list_atom,
                                   0, 1024, False, XA_ATOM,
                                   &actual_type, &actual_format,
                                   &nitems, &bytes_after, &data) == Success && data) {
                auto* atoms = reinterpret_cast<Atom*>(data);
                for (unsigned long i = 0; i < nitems; ++i) {
                    accepted_types_.push_back(atoms[i]);
                }
                XFree(data);
            }
        } else {
            for (int i = 2; i < 5 && ev.data.l[i]; ++i) {
                accepted_types_.push_back(static_cast<Atom>(ev.data.l[i]));
            }
        }

        pending_x_ = ev.data.l[2] >> 16;
        pending_y_ = ev.data.l[2] & 0xFFFF;

        handleDragEnter();

        sendXdndStatus(dpy, wnd, true);
    }

    void handleXdndPosition(Display* dpy, ::Window wnd, const XClientMessageEvent& ev) {
        pending_x_ = ev.data.l[2] >> 16;
        pending_y_ = ev.data.l[2] & 0xFFFF;

        ::Window root = RootWindow(dpy, DefaultScreen(dpy));
        ::Window child;
        int wx, wy;
        XTranslateCoordinates(dpy, root, wnd, pending_x_, pending_y_, &wx, &wy, &child);

        auto& dd = DragDropManager::instance();

        Atom action = static_cast<Atom>(ev.data.l[4]);

        KeyCode ctrl_l = XKeysymToKeycode(dpy, XK_Control_L);
        KeyCode ctrl_r = XKeysymToKeycode(dpy, XK_Control_R);
        char keymap[32];
        XQueryKeymap(dpy, keymap);
        bool ctrl_held = (keymap[ctrl_l / 8] & (1 << (ctrl_l % 8))) != 0 ||
                         (keymap[ctrl_r / 8] & (1 << (ctrl_r % 8))) != 0;

        DragOperation op = DragOperation::Copy;
        if (ctrl_held) op = DragOperation::Move;

        dd.onDragOver(wx, wy);

        sendXdndStatus(dpy, wnd, true);
    }

    void handleXdndDrop(Display* dpy, ::Window wnd, const XClientMessageEvent& ev) {
        if (xdnd_version_ >= 1) {
            Time timestamp = static_cast<Time>(ev.data.l[2]);
            pending_x_ = ev.data.l[2] >> 16;
            pending_y_ = ev.data.l[2] & 0xFFFF;

            requestXdndData(dpy, wnd);
        }
    }

    void handleXdndLeave(Display* dpy, ::Window /*wnd*/, const XClientMessageEvent& /*ev*/) {
        auto& dd = DragDropManager::instance();
        dd.onDragLeave();
        source_window_ = 0;
        accepted_types_.clear();
    }

    void sendXdndStatus(Display* dpy, ::Window wnd, bool accept) {
        if (!source_window_) return;

        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = source_window_;
        ev.xclient.message_type = XdndStatus;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = static_cast<long>(wnd);

        if (accept) {
            ev.xclient.data.l[1] = 1;
            ev.xclient.data.l[4] = static_cast<long>(XdndActionCopy);
        } else {
            ev.xclient.data.l[1] = 0;
        }

        XSendEvent(dpy, source_window_, False, NoEventMask, &ev);
        XFlush(dpy);
    }

    void requestXdndData(Display* dpy, ::Window wnd) {
        if (!source_window_ || accepted_types_.empty()) return;

        auto& ac = AtomCache::instance();
        Atom target = accepted_types_[0];

        Atom uri_list = ac.get(dpy, "text/uri-list");
        Atom plain = ac.get(dpy, "text/plain");
        Atom utf8 = ac.get(dpy, "UTF8_STRING");

        if (std::find(accepted_types_.begin(), accepted_types_.end(), uri_list) != accepted_types_.end()) {
            target = uri_list;
        } else if (std::find(accepted_types_.begin(), accepted_types_.end(), plain) != accepted_types_.end()) {
            target = plain;
        } else if (std::find(accepted_types_.begin(), accepted_types_.end(), utf8) != accepted_types_.end()) {
            target = utf8;
        }

        XConvertSelection(dpy, XdndSelection, target,
                          XdndSelection, wnd, CurrentTime);
    }

    void finishXdnd(Display* dpy, ::Window /*wnd*/) {
        if (!source_window_) return;

        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = source_window_;
        ev.xclient.message_type = XdndFinished;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 0;
        ev.xclient.data.l[1] = 0;
        ev.xclient.data.l[2] = 0;
        ev.xclient.data.l[3] = 0;
        ev.xclient.data.l[4] = 0;

        XSendEvent(dpy, source_window_, False, NoEventMask, &ev);
        XFlush(dpy);

        source_window_ = 0;
        accepted_types_.clear();
    }

    void handleDragEnter() {
        handleDragEnterCallback();
    }

    void handleDragEnterCallback() {
        auto& dd = DragDropManager::instance();
        dd.onDragEnter(pending_x_, pending_y_, DragOperation::Copy);
    }

    ::Window source_window_{0};
    unsigned long xdnd_version_{0};
    bool has_type_list_{false};
    std::vector<Atom> accepted_types_;
    int pending_x_{0};
    int pending_y_{0};
};

void DDLinuxWindow::handleDragDropEvent(const XEvent& ev) {
    auto& ac = AtomCache::instance();
    auto& handler = XDndHandler::instance();

    Atom msg_type = static_cast<Atom>(ev.xclient.message_type);

    if (msg_type == handler.XdndEnter ||
        msg_type == handler.XdndPosition ||
        msg_type == handler.XdndDrop ||
        msg_type == handler.XdndLeave) {
        handler.handleClientMessage(this, ev.xclient);
    } else if (ev.type == SelectionNotify) {
        handleSelectionNotify(ev);
    }
}

void DDLinuxWindow::handleSelectionNotify(const XEvent& ev) {
    auto& ac = AtomCache::instance();
    auto& handler = XDndHandler::instance();
    Display* dpy = display_;

    Atom prop = static_cast<Atom>(ev.xselection.property);
    if (prop == None) return;

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(dpy, window_, handler.XdndSelection,
                           0, 65536, False, AnyPropertyType,
                           &actual_type, &actual_format,
                           &nitems, &bytes_after, &data) == Success && data) {

        std::string content(reinterpret_cast<char*>(data), std::min(nitems, 65536UL));
        XFree(data);
        XDeleteProperty(dpy, window_, handler.XdndSelection);

        std::vector<DropItemData> items;

        if (actual_type == ac.get(dpy, "text/uri-list") ||
            content.find("file://") == 0) {
            std::string remaining = content;
            while (!remaining.empty()) {
                size_t nl = remaining.find("\r\n");
                if (nl == std::string::npos) nl = remaining.find('\n');
                std::string line = (nl != std::string::npos) ? remaining.substr(0, nl) : remaining;

                if (!line.empty() && line.find("file://") == 0) {
                    std::string path = line.substr(7);
                    while (!path.empty() && (path.back() == '\r' || path.back() == '\n')) {
                        path.pop_back();
                    }

                    for (size_t i = 0; i < path.size(); ++i) {
                        if (path[i] == '%' && i + 2 < path.size()) {
                            char hex[3] = {path[i+1], path[i+2], 0};
                            char decoded = static_cast<char>(std::strtol(hex, nullptr, 16));
                            path.replace(i, 3, 1, decoded);
                        }
                    }

                    DropItemData item;
                    item.type = DropDataType::File;
                    item.file_path = path;
                    items.push_back(std::move(item));
                }

                if (nl == std::string::npos) break;
                remaining = remaining.substr(nl + 1);
                if (!remaining.empty() && remaining[0] == '\n') remaining = remaining.substr(1);
            }
        } else {
            DropItemData item;
            item.type = DropDataType::Text;
            item.text = content;
            items.push_back(std::move(item));
        }

        if (drop_callback_ && !items.empty()) {
            std::vector<std::string> paths;
            for (const auto& it : items) {
                if (!it.file_path.empty()) paths.push_back(it.file_path);
                else if (!it.text.empty()) paths.push_back(it.text);
            }
            drop_callback_(std::move(paths));
        }

        auto& dd = DragDropManager::instance();
        dd.onDrop();

        handler.finishXdnd(dpy, window_);
    }
}

void DDLinuxWindow::handleSelectionNotify(const XEvent& ev);

} // namespace dd
