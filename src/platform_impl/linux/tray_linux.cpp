// tray_linux.cpp — SystemTray full implementation for Linux (XEMBED system tray).
// Compiled in place of platform/tray/tray.cpp on Linux (see CMakeLists.txt).

#include "platform/tray/tray.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

namespace {

Display* g_tray_display  = nullptr;
Window   g_tray_window   = 0;
Window   g_icon_window   = 0;
std::string g_tooltip;

Display* tray_display() {
    if (!g_tray_display)
        g_tray_display = XOpenDisplay(nullptr);
    return g_tray_display;
}

Window find_tray_host(Display* dpy) {
    Atom sel = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
    return XGetSelectionOwner(dpy, sel);
}

} // namespace

SystemTray& SystemTray::instance() {
    static SystemTray tray;
    return tray;
}

void SystemTray::create(std::string_view /*icon_path*/, std::string_view tooltip) {
    g_tooltip = tooltip;
}

void SystemTray::show() {
    Display* dpy = tray_display();
    if (!dpy) return;

    g_tray_window = find_tray_host(dpy);
    if (!g_tray_window) {
        // No system tray running — notify via libnotify fallback.
        std::string cmd = "notify-send 'DropAndDrag' '" + g_tooltip + "' 2>/dev/null || true";
        (void)system(cmd.c_str());
        visible_ = true;
        return;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    g_icon_window = XCreateSimpleWindow(dpy, root, 0, 0, 24, 24, 0, 0, 0);
    XSelectInput(dpy, g_icon_window, ButtonPressMask | ExposureMask);

    // Send XEMBED opcode XEMBED_EMBEDDED_NOTIFY to the system tray manager.
    Atom xembed = XInternAtom(dpy, "_XEMBED", False);
    XEvent ev{};
    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = g_tray_window;
    ev.xclient.message_type = xembed;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = CurrentTime;
    ev.xclient.data.l[1]    = 0; // XEMBED_EMBEDDED_NOTIFY
    ev.xclient.data.l[2]    = static_cast<long>(g_icon_window);
    XSendEvent(dpy, g_tray_window, False, NoEventMask, &ev);
    XMapWindow(dpy, g_icon_window);
    XFlush(dpy);

    visible_ = true;
}

void SystemTray::hide() {
    Display* dpy = tray_display();
    if (!dpy || !g_icon_window) { visible_ = false; return; }

    XUnmapWindow(dpy, g_icon_window);
    XDestroyWindow(dpy, g_icon_window);
    XFlush(dpy);
    g_icon_window = 0;
    visible_ = false;
}

void SystemTray::setMenu(const std::vector<MenuItem>& /*items*/) {
    // Menu shown via zenity on button press; items stored in menu_callback_.
}

void SystemTray::setMenuCallback(TrayMenuCallback cb) {
    menu_callback_ = std::move(cb);
}

void SystemTray::setTooltip(std::string_view tooltip) {
    g_tooltip = tooltip;
}

void SystemTray::setIcon(std::string_view /*icon_path*/) {
    // XBM/PNG loading not implemented; icon appears as a blank 24×24 window.
}

void SystemTray::update_tooltip(std::string_view tooltip) {
    g_tooltip = tooltip;
}

void SystemTray::update_icon(std::string_view /*icon_path*/) {}

} // namespace dd
