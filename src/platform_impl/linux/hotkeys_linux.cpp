// hotkeys_linux.cpp — HotkeyManager full implementation for Linux (X11 XGrabKey).
// Compiled in place of platform/hotkeys/hotkeys.cpp on Linux (see CMakeLists.txt).

#include "platform/hotkeys/hotkeys.hpp"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace dd {

namespace {

Display* g_hotkey_display = nullptr;
KeyCode  g_keycode        = 0;
unsigned int g_x11_mods   = 0;

Display* hotkey_display() {
    if (!g_hotkey_display) {
        g_hotkey_display = XOpenDisplay(nullptr);
        if (g_hotkey_display)
            XkbSetDetectableAutoRepeat(g_hotkey_display, True, nullptr);
    }
    return g_hotkey_display;
}

// Map Modifier bitmask → X11 modifier mask.
unsigned int to_x11_mods(uint8_t mods) {
    unsigned int r = 0;
    if (mods & Modifier::Ctrl)  r |= ControlMask;
    if (mods & Modifier::Alt)   r |= Mod1Mask;
    if (mods & Modifier::Shift) r |= ShiftMask;
    if (mods & Modifier::Meta)  r |= Mod4Mask;
    return r;
}

} // namespace

HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager mgr;
    return mgr;
}

bool HotkeyManager::registerHotkey(uint8_t modifiers, int key_code) {
    Display* dpy = hotkey_display();
    if (!dpy) return false;

    if (registered_) unregisterHotkey();

    // key_code is treated as XKeySym (matches parseHotkeyString which stores
    // std::toupper(char) — same as ASCII-range XK_* values).
    KeyCode kc = XKeysymToKeycode(dpy, static_cast<KeySym>(key_code));
    if (kc == 0) return false;

    unsigned int mods = to_x11_mods(modifiers);
    Window root = DefaultRootWindow(dpy);

    // Grab with the base modifier combo and common lock-key variants so the
    // hotkey fires regardless of NumLock / CapsLock state.
    auto grab = [&](unsigned int m) {
        XGrabKey(dpy, kc, m, root, True, GrabModeAsync, GrabModeAsync);
    };
    grab(mods);
    grab(mods | LockMask);
    grab(mods | Mod2Mask);
    grab(mods | LockMask | Mod2Mask);

    XFlush(dpy);

    g_keycode   = kc;
    g_x11_mods  = mods;
    registered_ = true;
    return true;
}

void HotkeyManager::unregisterHotkey() {
    Display* dpy = hotkey_display();
    if (!dpy || !registered_) return;

    Window root = DefaultRootWindow(dpy);
    auto ungrab = [&](unsigned int m) {
        XUngrabKey(dpy, g_keycode, m, root);
    };
    ungrab(g_x11_mods);
    ungrab(g_x11_mods | LockMask);
    ungrab(g_x11_mods | Mod2Mask);
    ungrab(g_x11_mods | LockMask | Mod2Mask);

    XFlush(dpy);
    registered_ = false;
}

bool HotkeyManager::isRegistered() const {
    return registered_;
}

void HotkeyManager::setCallback(HotkeyCallback cb) {
    callback_ = std::move(cb);
}

bool HotkeyManager::check_key_event(void* event_ptr) {
    if (!registered_ || !callback_) return false;
    auto* ke = static_cast<XKeyEvent*>(event_ptr);
    if (ke->type != KeyPress) return false;
    unsigned int state = ke->state & (ControlMask | ShiftMask | Mod1Mask | Mod4Mask);
    if (ke->keycode == g_keycode && state == g_x11_mods) {
        callback_();
        return true;
    }
    return false;
}

std::optional<HotkeyDefinition> HotkeyManager::parseHotkeyString(std::string_view str) {
    HotkeyDefinition def;
    std::string s{str};
    std::istringstream ss(s);
    std::string token;

    while (std::getline(ss, token, '+')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);

        auto lower = std::string(token.size(), '\0');
        std::transform(token.begin(), token.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower == "ctrl" || lower == "control") {
            def.modifiers = def.modifiers | Modifier::Ctrl;
        } else if (lower == "alt") {
            def.modifiers = def.modifiers | Modifier::Alt;
        } else if (lower == "shift") {
            def.modifiers = def.modifiers | Modifier::Shift;
        } else if (lower == "meta" || lower == "cmd" || lower == "command" ||
                   lower == "win" || lower == "windows") {
            def.modifiers = def.modifiers | Modifier::Meta;
        } else {
            if (token.length() == 1) {
                def.key_code = static_cast<int>(std::toupper(static_cast<unsigned char>(token[0])));
            } else {
                def.key_code = 0;
            }
        }
    }

    def.display_string = std::string(str);
    return def;
}

} // namespace dd
