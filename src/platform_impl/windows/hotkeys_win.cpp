// hotkeys_win.cpp — Windows global hotkeys stub (RegisterHotKey implementation pending).

#include "platform/hotkeys/hotkeys.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string_view>

namespace dd {
namespace win {

namespace {

constexpr UINT kHotkeyId = 0xDA00;

UINT mapModifiers(uint8_t mods) {
    UINT result = 0;
    if (mods & static_cast<uint8_t>(Modifier::Ctrl))  result |= MOD_CONTROL;
    if (mods & static_cast<uint8_t>(Modifier::Alt))    result |= MOD_ALT;
    if (mods & static_cast<uint8_t>(Modifier::Shift))  result |= MOD_SHIFT;
    if (mods & static_cast<uint8_t>(Modifier::Meta))   result |= MOD_WIN;
    return result;
}

uint8_t unmapModifiers(UINT mods) {
    uint8_t result = 0;
    if (mods & MOD_CONTROL) result |= static_cast<uint8_t>(Modifier::Ctrl);
    if (mods & MOD_ALT)     result |= static_cast<uint8_t>(Modifier::Alt);
    if (mods & MOD_SHIFT)   result |= static_cast<uint8_t>(Modifier::Shift);
    if (mods & MOD_WIN)     result |= static_cast<uint8_t>(Modifier::Meta);
    return result;
}

} // namespace

static std::atomic<bool> g_hotkeyRegistered{false};
static std::function<void()> g_hotkeyCallback;
static HWND g_hotkeyHwnd = nullptr;

bool registerHotkey(HWND hwnd, uint8_t modifiers, int key_code) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    UINT fsModifiers = mapModifiers(modifiers);
    UINT vk = static_cast<UINT>(key_code);
    if (vk == 0) return false;

    if (g_hotkeyRegistered.load()) {
        UnregisterHotKey(g_hotkeyHwnd, kHotkeyId);
    }

    BOOL ok = RegisterHotKey(hwnd, kHotkeyId, fsModifiers, vk);
    if (ok) {
        g_hotkeyHwnd = hwnd;
        g_hotkeyRegistered.store(true);
    }
    return ok != 0;
}

void unregisterHotkey() {
    if (g_hotkeyRegistered.exchange(false)) {
        if (g_hotkeyHwnd && IsWindow(g_hotkeyHwnd)) {
            UnregisterHotKey(g_hotkeyHwnd, kHotkeyId);
        }
        g_hotkeyHwnd = nullptr;
    }
}

void setHotkeyCallback(std::function<void()> cb) {
    g_hotkeyCallback = std::move(cb);
}

bool isHotkeyRegistered() {
    return g_hotkeyRegistered.load();
}

void handleHotkeyMessage(WPARAM /*wp*/, LPARAM /*lp*/) {
    if (g_hotkeyCallback) {
        g_hotkeyCallback();
    }
}

std::optional<HotkeyDefinition> parseHotkeyString(std::string_view str) {
    return HotkeyManager::parseHotkeyString(str);
}

} // namespace win
} // namespace dd
