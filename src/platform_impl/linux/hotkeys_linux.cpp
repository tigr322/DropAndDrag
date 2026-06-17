#include "platform/hotkeys/hotkeys.hpp"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <cctype>
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
    if (dpy) XkbSetDetectableAutoRepeat(dpy, True, nullptr);
    return dpy;
}

Window get_root() {
    return DefaultRootWindow(get_display());
}

KeyCode keysym_to_keycode(KeySym keysym) {
    return XKeysymToKeycode(get_display(), keysym);
}

unsigned int parse_modifiers(std::string_view spec) {
    unsigned int mod = 0;
    if (spec.find("Ctrl") != std::string_view::npos) mod |= ControlMask;
    if (spec.find("Alt") != std::string_view::npos) mod |= Mod1Mask;
    if (spec.find("Shift") != std::string_view::npos) mod |= ShiftMask;
    if (spec.find("Meta") != std::string_view::npos || spec.find("Super") != std::string_view::npos)
        mod |= Mod4Mask;
    return mod;
}

KeySym parse_key(std::string_view spec) {
    size_t last_plus = spec.rfind('+');
    std::string_view key_name = (last_plus != std::string_view::npos)
        ? spec.substr(last_plus + 1)
        : spec;

    if (key_name.size() == 1) {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(key_name[0])));
        return static_cast<KeySym>(c);
    }

    if (key_name == "Space") return XK_space;
    if (key_name == "Return" || key_name == "Enter") return XK_Return;
    if (key_name == "Escape" || key_name == "Esc") return XK_Escape;
    if (key_name == "Tab") return XK_Tab;
    if (key_name == "BackSpace") return XK_BackSpace;
    if (key_name == "Delete") return XK_Delete;
    if (key_name == "Left") return XK_Left;
    if (key_name == "Right") return XK_Right;
    if (key_name == "Up") return XK_Up;
    if (key_name == "Down") return XK_Down;
    if (key_name == "Home") return XK_Home;
    if (key_name == "End") return XK_End;
    if (key_name == "PageUp") return XK_Page_Up;
    if (key_name == "PageDown") return XK_Page_Down;
    if (key_name == "F1") return XK_F1;
    if (key_name == "F2") return XK_F2;
    if (key_name == "F3") return XK_F3;
    if (key_name == "F4") return XK_F4;
    if (key_name == "F5") return XK_F5;
    if (key_name == "F6") return XK_F6;
    if (key_name == "F7") return XK_F7;
    if (key_name == "F8") return XK_F8;
    if (key_name == "F9") return XK_F9;
    if (key_name == "F10") return XK_F10;
    if (key_name == "F11") return XK_F11;
    if (key_name == "F12") return XK_F12;

    return XStringToKeysym(key_name.data());
}

} // namespace

HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager mgr;
    return mgr;
}

bool HotkeyManager::register_hotkey(std::string_view hotkey_spec) {
    auto display = get_display();
    if (!display) return false;

    if (is_registered_) {
        unregister_hotkey();
    }

    unsigned int modifiers = parse_modifiers(hotkey_spec);
    KeySym keysym = parse_key(hotkey_spec);
    KeyCode keycode = keysym_to_keycode(keysym);

    if (keycode == 0) return false;

    Window root = get_root();

    XGrabKey(display, keycode, modifiers, root, True, GrabModeAsync, GrabModeAsync);

    if (modifiers & ControlMask) {
        XGrabKey(display, keycode, modifiers | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(display, keycode, modifiers | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(display, keycode, modifiers | LockMask | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
    }

    XFlush(display);

    registered_modifiers_ = modifiers;
    registered_keycode_ = keycode;
    is_registered_ = true;
    return true;
}

bool HotkeyManager::unregister_hotkey() {
    auto display = get_display();
    if (!display || !is_registered_) return false;

    Window root = get_root();
    XUngrabKey(display, registered_keycode_, registered_modifiers_, root);

    if (registered_modifiers_ & ControlMask) {
        XUngrabKey(display, registered_keycode_, registered_modifiers_ | LockMask, root);
        XUngrabKey(display, registered_keycode_, registered_modifiers_ | Mod2Mask, root);
        XUngrabKey(display, registered_keycode_, registered_modifiers_ | LockMask | Mod2Mask, root);
    }

    XFlush(display);
    is_registered_ = false;
    return true;
}

bool HotkeyManager::is_registered() const {
    return is_registered_;
}

bool HotkeyManager::check_key_event(void* event_ptr) {
    if (!is_registered_ || !on_hotkey_) return false;

    auto key_event = static_cast<XKeyEvent*>(event_ptr);
    if (key_event->type != KeyPress) return false;

    unsigned int state = key_event->state & (ControlMask | ShiftMask | Mod1Mask | Mod4Mask);
    if (key_event->keycode == registered_keycode_ && state == registered_modifiers_) {
        on_hotkey_();
        return true;
    }
    return false;
}

void HotkeyManager::set_callback(std::function<void()> callback) {
    on_hotkey_ = std::move(callback);
}

} // namespace dd
