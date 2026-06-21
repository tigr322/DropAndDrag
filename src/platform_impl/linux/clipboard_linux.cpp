// clipboard_linux.cpp — Linux clipboard implementation via X11 selections.

#include "platform/clipboard/clipboard.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

namespace {

std::mutex g_clipboard_mutex;
std::vector<uint8_t> g_clipboard_data;

Display* clipboard_display() {
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

Atom utf8_atom()      { static Atom a = XInternAtom(clipboard_display(), "UTF8_STRING",  False); return a; }
Atom clipboard_atom() { static Atom a = XInternAtom(clipboard_display(), "CLIPBOARD",    False); return a; }
Atom primary_atom()   { static Atom a = XInternAtom(clipboard_display(), "PRIMARY",      False); return a; }
Atom targets_atom()   { static Atom a = XInternAtom(clipboard_display(), "TARGETS",      False); return a; }

} // namespace

ClipboardManager& ClipboardManager::instance() {
    static ClipboardManager mgr;
    return mgr;
}

void ClipboardManager::copy(std::string_view text) {
    std::lock_guard lock(g_clipboard_mutex);
    Display* dpy = clipboard_display();
    if (!dpy) return;

    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
    XSetSelectionOwner(dpy, clipboard_atom(), win, CurrentTime);
    XSetSelectionOwner(dpy, primary_atom(),   win, CurrentTime);

    g_clipboard_data.assign(text.begin(), text.end());

    XDestroyWindow(dpy, win);
    XFlush(dpy);
}

void ClipboardManager::copyFile(std::string_view path) {
    std::lock_guard lock(g_clipboard_mutex);
    Display* dpy = clipboard_display();
    if (!dpy) return;

    std::string uri = "file://";
    uri += path;
    uri += "\r\n";

    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
    XSetSelectionOwner(dpy, clipboard_atom(), win, CurrentTime);
    g_clipboard_data.assign(uri.begin(), uri.end());

    XDestroyWindow(dpy, win);
    XFlush(dpy);
}

void ClipboardManager::copyImage(const std::vector<uint8_t>& /*data*/) {}

std::string ClipboardManager::paste() const {
    std::lock_guard lock(g_clipboard_mutex);
    Display* dpy = clipboard_display();
    if (!dpy) return {};

    Atom utf8      = utf8_atom();
    Atom clipboard = clipboard_atom();

    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
    XConvertSelection(dpy, clipboard, utf8, utf8, win, CurrentTime);

    XEvent event;
    std::string result;
    for (int i = 0; i < 100; ++i) {
        if (XCheckTypedWindowEvent(dpy, win, SelectionNotify, &event)) {
            if (event.xselection.property != None) {
                Atom actual_type;
                int actual_format;
                unsigned long nitems, bytes_after;
                unsigned char* data = nullptr;
                XGetWindowProperty(dpy, win, utf8, 0, 65536, True, AnyPropertyType,
                                   &actual_type, &actual_format, &nitems, &bytes_after, &data);
                if (data) {
                    result.assign(reinterpret_cast<char*>(data), nitems);
                    XFree(data);
                }
            }
            break;
        }
        XFlush(dpy);
    }

    XDestroyWindow(dpy, win);
    return result;
}

bool ClipboardManager::hasText() const {
    Display* dpy = clipboard_display();
    if (!dpy) return false;

    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
    XConvertSelection(dpy, clipboard_atom(), targets_atom(), targets_atom(), win, CurrentTime);

    Atom utf8 = utf8_atom();
    bool has = false;
    XEvent event;
    for (int i = 0; i < 100; ++i) {
        if (XCheckTypedWindowEvent(dpy, win, SelectionNotify, &event)) {
            if (event.xselection.property != None) {
                Atom actual_type;
                int actual_format;
                unsigned long nitems, bytes_after;
                unsigned char* data = nullptr;
                XGetWindowProperty(dpy, win, targets_atom(), 0, 65536, True, XA_ATOM,
                                   &actual_type, &actual_format, &nitems, &bytes_after, &data);
                if (data) {
                    auto* atoms = reinterpret_cast<Atom*>(data);
                    for (unsigned long j = 0; j < nitems; ++j) {
                        if (atoms[j] == utf8) { has = true; break; }
                    }
                    XFree(data);
                }
            }
            break;
        }
        XFlush(dpy);
    }

    XDestroyWindow(dpy, win);
    return has;
}

bool ClipboardManager::hasImage() const {
    return false;
}

} // namespace dd
