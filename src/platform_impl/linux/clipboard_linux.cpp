// clipboard_linux.cpp — Linux clipboard stub (X11/Wayland implementation pending).

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
Display* get_display() {
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

Atom get_utf8_atom() {
    static Atom atom = XInternAtom(get_display(), "UTF8_STRING", False);
    return atom;
}

Atom get_clipboard_atom() {
    static Atom atom = XInternAtom(get_display(), "CLIPBOARD", False);
    return atom;
}

Atom get_primary_atom() {
    static Atom atom = XInternAtom(get_display(), "PRIMARY", False);
    return atom;
}

Atom get_targets_atom() {
    static Atom atom = XInternAtom(get_display(), "TARGETS", False);
    return atom;
}

std::mutex clipboard_mutex;
} // namespace

ClipboardManager& ClipboardManager::instance() {
    static ClipboardManager mgr;
    return mgr;
}

void ClipboardManager::copy(std::string_view text) {
    std::lock_guard lock(clipboard_mutex);
    auto display = get_display();
    if (!display) return;

    auto utf8 = get_utf8_atom();
    auto clipboard = get_clipboard_atom();
    auto primary = get_primary_atom();

    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                       0, 0, 1, 1, 0, 0, 0);

    std::vector<char> data(text.begin(), text.end());
    data.push_back('\0');

    XSetSelectionOwner(display, clipboard, window, CurrentTime);
    XSetSelectionOwner(display, primary, window, CurrentTime);

    clipboard_data_.assign(text.begin(), text.end());

    XDestroyWindow(display, window);
    XFlush(display);
}

void ClipboardManager::copyFile(std::string_view path) {
    std::lock_guard lock(clipboard_mutex);
    auto display = get_display();
    if (!display) return;

    std::string uri_list = "file://";
    uri_list += path;
    uri_list += "\r\n";

    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                       0, 0, 1, 1, 0, 0, 0);

    auto uri_atom = XInternAtom(display, "text/uri-list", False);
    auto clipboard = get_clipboard_atom();

    XSetSelectionOwner(display, clipboard, window, CurrentTime);
    clipboard_data_ = std::vector<uint8_t>(uri_list.begin(), uri_list.end());

    XDestroyWindow(display, window);
    XFlush(display);
}

void ClipboardManager::copyImage(const std::vector<uint8_t>& /*data*/) {
}

std::string ClipboardManager::paste() {
    std::lock_guard lock(clipboard_mutex);
    auto display = get_display();
    if (!display) return {};

    auto utf8 = get_utf8_atom();
    auto clipboard = get_clipboard_atom();

    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                       0, 0, 1, 1, 0, 0, 0);

    XConvertSelection(display, clipboard, utf8, utf8, window, CurrentTime);

    XEvent event;
    for (int i = 0; i < 100; ++i) {
        if (XCheckTypedWindowEvent(display, window, SelectionNotify, &event)) {
            if (event.xselection.property != None) {
                Atom actual_type;
                int actual_format;
                unsigned long nitems, bytes_after;
                unsigned char* data = nullptr;

                XGetWindowProperty(display, window, utf8,
                                 0, 65536, True, AnyPropertyType,
                                 &actual_type, &actual_format,
                                 &nitems, &bytes_after, &data);

                std::string result;
                if (data) {
                    result.assign(reinterpret_cast<char*>(data), nitems);
                    XFree(data);
                }

                XDestroyWindow(display, window);
                return result;
            }
            break;
        }
        XFlush(display);
    }

    XDestroyWindow(display, window);
    return {};
}

bool ClipboardManager::has_text() {
    auto display = get_display();
    if (!display) return false;

    auto utf8 = get_utf8_atom();
    auto clipboard = get_clipboard_atom();

    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                       0, 0, 1, 1, 0, 0, 0);
    XConvertSelection(display, clipboard, get_targets_atom(), get_targets_atom(),
                     window, CurrentTime);

    XEvent event;
    bool has_utf8 = false;
    for (int i = 0; i < 100; ++i) {
        if (XCheckTypedWindowEvent(display, window, SelectionNotify, &event)) {
            if (event.xselection.property != None) {
                Atom actual_type;
                int actual_format;
                unsigned long nitems, bytes_after;
                unsigned char* data = nullptr;
                XGetWindowProperty(display, window, get_targets_atom(),
                                 0, 65536, True, XA_ATOM,
                                 &actual_type, &actual_format,
                                 &nitems, &bytes_after, &data);
                if (data) {
                    auto atoms = reinterpret_cast<Atom*>(data);
                    for (unsigned long i = 0; i < nitems; ++i) {
                        if (atoms[i] == utf8) {
                            has_utf8 = true;
                            break;
                        }
                    }
                    XFree(data);
                }
            }
            break;
        }
        XFlush(display);
    }

    XDestroyWindow(display, window);
    return has_utf8;
}

bool ClipboardManager::has_image() {
    return false;
}

std::vector<uint8_t> ClipboardManager::get_clipboard_data() const {
    return clipboard_data_;
}

} // namespace dd
