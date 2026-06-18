// tray_linux.cpp — Linux system tray stub (libappindicator/StatusNotifierItem pending).

#include "platform/tray/tray.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

namespace {

Display* get_display() {
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

Atom get_systray_atom() {
    static Atom atom = XInternAtom(get_display(), "_NET_SYSTEM_TRAY_S0", False);
    return atom;
}

Atom get_xembed_atom() {
    static Atom atom = XInternAtom(get_display(), "_XEMBED", False);
    return atom;
}

Atom get_xembed_info_atom() {
    static Atom atom = XInternAtom(get_display(), "_XEMBED_INFO", False);
    return atom;
}

Window find_tray_window() {
    auto display = get_display();
    auto selection = get_systray_atom();
    return XGetSelectionOwner(display, selection);
}

} // namespace

SystemTray& SystemTray::instance() {
    static SystemTray tray;
    return tray;
}

void SystemTray::create(std::string_view icon_path, std::string_view tooltip) {
    tooltip_ = tooltip;
}

void SystemTray::show() {
    auto display = get_display();
    if (!display) return;

    tray_window_ = find_tray_window();
    if (!tray_window_) {
        notify_fallback("Show", tooltip_);
        return;
    }

    Window root = DefaultRootWindow(display);
    int screen = DefaultScreen(display);
    icon_window_ = XCreateSimpleWindow(display, root, 0, 0, 24, 24, 0, 0, 0);

    XSelectInput(display, icon_window_, ButtonPressMask | ExposureMask | StructureNotifyMask);

    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray_window_;
    ev.xclient.message_type = get_xembed_atom();
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = 0;
    ev.xclient.data.l[2] = icon_window_;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;

    XSendEvent(display, tray_window_, False, NoEventMask, &ev);
    XMapWindow(display, icon_window_);
    XFlush(display);

    is_visible_ = true;
}

void SystemTray::hide() {
    auto display = get_display();
    if (!display || !icon_window_) return;

    XUnmapWindow(display, icon_window_);
    XDestroyWindow(display, icon_window_);
    icon_window_ = 0;
    is_visible_ = false;
}

void SystemTray::set_menu(std::vector<MenuItem> items) {
    menu_items_ = std::move(items);
}

void SystemTray::set_menu_callback(std::function<void(std::string_view)> callback) {
    menu_callback_ = std::move(callback);
}

void SystemTray::update_icon(std::string_view icon_path) {
}

void SystemTray::update_tooltip(std::string_view tooltip) {
    tooltip_ = tooltip;
}

void SystemTray::notify_fallback(std::string_view action, std::string_view detail) {
    std::string cmd = "notify-send 'DropAndDrag' '" + std::string(detail) + "'";
    system(cmd.c_str());
}

bool SystemTray::is_visible() const {
    return is_visible_;
}

Window SystemTray::get_icon_window() const {
    return icon_window_;
}

void SystemTray::handle_button_press(int x, int y, int button) {
    if (button == 3 && menu_callback_) {
        auto display = get_display();
        if (!display) return;

        const char* cmd = "zenity --list --title='DropAndDrag' "
                         "--column='Action' 'Show/Hide' 'Settings' 'Quit' "
                         "--width=200 --height=200";
        FILE* pipe = popen(cmd, "r");
        if (pipe) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                std::string selected(buffer);
                selected.erase(selected.find_last_not_of("\n\r") + 1);
                if (selected == "Show/Hide") menu_callback_("toggle");
                else if (selected == "Quit") menu_callback_("quit");
            }
            pclose(pipe);
        }
    } else if (button == 1 && menu_callback_) {
        menu_callback_("toggle");
    }
}

} // namespace dd
